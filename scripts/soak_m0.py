#!/usr/bin/env python3
"""Observe HTTP/BLE health while keeping three HAP TCP sockets open."""
import argparse
from contextlib import ExitStack
from datetime import datetime, timezone
import json
from pathlib import Path
import socket
import time
import requests

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--context', type=Path, required=True, help='Private JSON containing device host')
parser.add_argument('--seconds', type=int, default=1800)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.seconds <= 0:
    parser.error('--seconds must be positive')
host = json.loads(args.context.read_text())['host']
args.output.parent.mkdir(parents=True, exist_ok=True)
samples = failures = 0
heaps = []
last_uptime = None
first_sample_at = last_sample_at = None
consecutive_failures = 0
session = requests.Session()
session.trust_env = False
with ExitStack() as stack, args.output.open('w') as log:
    connections = [stack.enter_context(socket.create_connection((host, 1201), timeout=5))
                   for _ in range(3)]
    started = time.monotonic()
    start_utc = datetime.now(timezone.utc).isoformat()
    last_scan = last_report = started
    while True:
        elapsed = time.monotonic() - started
        try:
            response = session.get('http://' + host + '/health', timeout=5)
            response.raise_for_status()
            state = response.json()
            assert state['firmware'] == 'm0-smoke', 'unexpected firmware'
            assert state['wifi_connected'] and state['ble_initialized'], 'network or BLE not ready'
            assert state['fridge_connected'] is False, 'smoke connected to a fridge'
            uptime = state['uptime_ms']
            assert last_uptime is None or uptime >= last_uptime, 'unexpected device reset'
            last_uptime = uptime
            if state['scan_running']:
                last_scan = time.monotonic()
            assert time.monotonic() - last_scan <= 10, 'scanning did not resume'
            for connection in connections:
                connection.settimeout(0.01)
                try:
                    assert connection.recv(1, socket.MSG_PEEK) != b'', 'HAP connection closed'
                except socket.timeout:
                    pass
            last_sample_at = time.monotonic()
            if first_sample_at is None:
                first_sample_at = last_sample_at
            samples += 1
            heaps.append(state['free_heap'])
            consecutive_failures = 0
            log.write(json.dumps({'elapsed_s': round(last_sample_at - started, 2), 'state': state}) + '\n')
        except Exception as error:
            failures += 1
            consecutive_failures += 1
            # No URL, IP, credentials or response bodies in shared failure output.
            log.write(json.dumps({'elapsed_s': round(elapsed, 2), 'error_type': type(error).__name__}) + '\n')
        log.flush()
        if time.monotonic() - last_report >= 60:
            print(json.dumps({'elapsed_s': round(time.monotonic()-started), 'samples': samples,
                              'failures': failures, 'free_heap': heaps[-1] if heaps else None}), flush=True)
            last_report = time.monotonic()
        if consecutive_failures >= 5:
            break
        if first_sample_at is not None and last_sample_at - first_sample_at >= args.seconds:
            break
        time.sleep(5)
summary = {'started_utc': start_utc, 'elapsed_s': round(time.monotonic()-started, 2),
           'required_s': args.seconds, 'accepted_span_s': round(last_sample_at-first_sample_at, 2) if first_sample_at is not None else 0,
           'samples': samples, 'failures': failures,
           'free_heap_first': heaps[0] if heaps else None,
           'free_heap_last': heaps[-1] if heaps else None,
           'free_heap_min': min(heaps) if heaps else None,
           'passed': failures == 0 and first_sample_at is not None and last_sample_at-first_sample_at >= args.seconds and samples > 0}
args.output.with_suffix('.summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps(summary), flush=True)
raise SystemExit(0 if summary['passed'] else 1)

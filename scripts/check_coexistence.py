#!/usr/bin/env python3
"""Check /health while holding three HAP TCP connections open."""
import argparse
import json
import socket
import time
from contextlib import ExitStack
from urllib.request import urlopen

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('host', help='ESP32 IP address or mDNS name')
parser.add_argument('--seconds', type=int, default=30)
args = parser.parse_args()
if args.seconds <= 0:
    parser.error('--seconds must be positive')
with ExitStack() as stack:
    sockets = [stack.enter_context(socket.create_connection((args.host, 1201), timeout=5))
               for _ in range(3)]
    deadline = time.monotonic() + args.seconds
    samples = 0
    last_scanning = time.monotonic()
    saw_scanning = False
    while time.monotonic() < deadline:
        with urlopen('http://' + args.host + '/health', timeout=5) as response:
            state = json.load(response)
        if not state.get('wifi_connected') or not state.get('ble_initialized'):
            raise SystemExit('FAIL: network or BLE not ready')
        if state.get('scan_running'):
            saw_scanning = True
            last_scanning = time.monotonic()
        elif time.monotonic() - last_scanning > 3:
            raise SystemExit('FAIL: BLE scanning did not resume within 3 seconds')
        if state.get('fridge_connected') is not False:
            raise SystemExit('FAIL: smoke must not connect to a fridge')
        for connection in sockets:
            connection.settimeout(0.01)
            try:
                if connection.recv(1, socket.MSG_PEEK) == b'':
                    raise SystemExit('FAIL: HAP TCP socket was closed during check')
            except socket.timeout:
                pass
        samples += 1
        time.sleep(1)
    if not saw_scanning:
        raise SystemExit('FAIL: no active BLE scanning observed')
    print('PASS:', samples, 'HTTP samples with 3 concurrent HAP TCP sockets.')
    print('This verifies TCP coexistence, not authenticated HomeKit pairing.')

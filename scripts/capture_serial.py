#!/usr/bin/env python3
"""Capture smoke diagnostics to a local file; optional hardware reset."""
import argparse
from pathlib import Path
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', required=True)
parser.add_argument('--seconds', type=float, default=20)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--reset', action='store_true')
args = parser.parse_args()
if args.seconds <= 0:
    parser.error('--seconds must be positive')
args.output.parent.mkdir(parents=True, exist_ok=True)
with serial.Serial(args.port, 115200, timeout=1) as port, args.output.open('w') as output:
    port.dtr = False
    port.rts = False
    if args.reset:
        port.rts = True
        time.sleep(0.1)
        port.rts = False
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        line = port.readline().decode(errors='replace')
        output.write(line)
        output.flush()
        if line.startswith(('FrostBridge M0', 'M0 coexistence', 'uptime_ms=')):
            print(line.strip(), flush=True)

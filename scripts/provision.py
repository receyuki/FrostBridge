#!/usr/bin/env python3
"""Local USB Wi-Fi setup; passwords are never arguments or printed."""
import argparse
import getpass
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', required=True)
parser.add_argument('--ssid', help='Wi-Fi name; passwords are never accepted as arguments')
parser.add_argument('--keep-password', action='store_true', help='Change SSID only; keep the password on ESP32')
args = parser.parse_args()
ssid = args.ssid if args.ssid is not None else input('2.4GHz Wi-Fi SSID: ')
password = '' if args.keep_password else getpass.getpass('Wi-Fi password: ')
if not 1 <= len(ssid.encode()) <= 32:
    parser.error('SSID must be 1–32 bytes.')
if not args.keep_password and not 8 <= len(password.encode()) <= 63:
    parser.error('Password must be 8–63 bytes.')
if any(c in ssid + password for c in '\t\r\n\0'):
    parser.error('Tabs, line breaks and NUL are unsupported.')
with serial.Serial(args.port, 115200, timeout=1) as port:
    port.dtr = False
    port.rts = False
    time.sleep(4)
    port.reset_input_buffer()
    command = ('SSID\t' + ssid if args.keep_password else 'WIFI\t' + ssid + '\t' + password)
    port.write((command + '\n').encode())
    command = ''
    password = ''
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        line = port.readline().decode(errors='replace').strip()
        if line == 'M0_CONFIG_SAVED':
            print('Saved locally on ESP32; it is restarting. Credentials were not logged.')
            break
        if line == 'M0_CONFIG_REJECTED':
            raise SystemExit('Device rejected configuration.')
    else:
        raise SystemExit('No acknowledgement; check coexistence firmware and serial port.')

#!/usr/bin/env python3
"""Check an unpaired FrostBridge through real HAP; never change fridge settings.

Requires isolated aiohomekit 3.2.20. Creates a diagnostic pairing, validates its
accessory database and same-name metadata write, then removes only that pairing.
Secrets and full databases stay in the caller's ignored local output directory.
"""
import argparse
import asyncio
import json
from pathlib import Path
import re
from urllib.request import urlopen

from zeroconf.asyncio import AsyncServiceInfo, AsyncZeroconf
from aiohomekit.characteristic_cache import CharacteristicCacheMemory
from aiohomekit.controller.ip.controller import IpController
from aiohomekit.controller.ip.discovery import IpDiscovery
from aiohomekit.zeroconf import HomeKitService

async def check(args):
    local = Path(__file__).resolve().parents[1] / '.local'
    if not args.output.resolve().is_relative_to(local.resolve()):
        raise ValueError('Private output must be inside this repository .local/')
    args.output.mkdir(parents=True, exist_ok=True, mode=0o700)
    args.output.chmod(0o700)
    host = json.loads(args.context.read_text())['host']
    def snapshot():
        with urlopen('http://' + host + '/api/v1/status', timeout=4) as response:
            return json.load(response)
    initial = snapshot()
    if initial['homekit']['paired']:
        raise RuntimeError('Refuse: already paired; leave existing controllers alone')
    info = AsyncServiceInfo('_hap._tcp.local.', 'FrostBridge._hap._tcp.local.')
    async with AsyncZeroconf() as zc:
        if not await info.async_request(zc.zeroconf, 5000):
            raise RuntimeError('FrostBridge discovery failed')
        description = HomeKitService.from_service_info(info)
        if host not in description.addresses:
            raise RuntimeError('Discovered service is not the selected gateway')
        controller = IpController(CharacteristicCacheMemory(), zc)
        discovery = IpDiscovery(controller, description)
        pairing = None
        try:
            finish = await asyncio.wait_for(discovery.async_start_pairing('frostbridge-diagnostic'), 20)
            code = json.loads(args.pairing.read_text())['code']
            pairing = await asyncio.wait_for(finish(f'{code[:3]}-{code[3:5]}-{code[5:]}'), 20)
            private = args.output / 'diagnostic-pairing-private.json'
            private.write_text(json.dumps(pairing.pairing_data, indent=2) + '\n')
            private.chmod(0o600)
            data = await asyncio.wait_for(pairing.list_accessories_and_characteristics(), 20)
            (args.output / 'accessories-private.json').write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
            if len(data) != args.expected_accessories:
                raise RuntimeError('Unexpected accessory count')
            chars = [c for a in data for s in a['services'] for c in s['characteristics']]
            versions = [c['value'] for c in chars if c['type'].startswith('00000052-')]
            if len(versions) != args.expected_accessories or not all(re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}', v) for v in versions):
                raise RuntimeError('Invalid FirmwareRevision')
            print('PASS: authenticated database, accessory count and numeric FirmwareRevision', flush=True)
            name = next(c for a in data if a['aid'] == 4 for s in a['services'] for c in s['characteristics'] if c['type'].startswith('000000E3-'))
            before = snapshot()
            result = await pairing.put_characteristics([(4, name['iid'], name['value'])])
            await asyncio.sleep(0.8)
            after = snapshot()
            if result:
                raise RuntimeError('Metadata write rejected')
            if after['homekit']['automation_requests'] != before['homekit']['automation_requests']:
                raise RuntimeError('Name write became an automation control')
            for key in ('power', 'mode', 'target_c'):
                if after['device'][key] != before['device'][key]:
                    raise RuntimeError('Name write changed fridge setting')
            print('PASS: ConfiguredName write produces no automation request or setting change', flush=True)
        finally:
            pairing = pairing or controller.pairings.get('frostbridge-diagnostic')
            try:
                if pairing:
                    try:
                        await asyncio.wait_for(pairing.remove_pairing(pairing.pairing_data['iOSPairingId']), 10)
                        print('CLEANUP: removed only diagnostic controller', flush=True)
                    finally:
                        await pairing.close()
            finally:
                try:
                    await discovery.close()
                finally:
                    if pairing and snapshot()['homekit']['paired']:
                        raise RuntimeError('Diagnostic pairing cleanup not confirmed')
    print('PASS: gateway unpaired for iPhone; Home App acceptance remains separate', flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--context', type=Path, required=True)
    parser.add_argument('--pairing', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-accessories', type=int, default=9)
    asyncio.run(check(parser.parse_args()))

#!/usr/bin/env python3
"""Exercise the public package/install CLI without opening a real serial port."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent


class PrebuiltCLI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='frostbridge-cli-test-')
        cls.directory = Path(cls.temp.name)
        cls.archive = cls.directory / 'package.zip'
        subprocess.run([sys.executable, str(ROOT / 'frostbridge.py'), 'package', '--version', 'test.1',
                        '--output', str(cls.archive)], check=True, stdout=subprocess.PIPE)
        with zipfile.ZipFile(cls.archive) as archive:
            archive.extractall(cls.directory / 'original')
        cls.original = cls.directory / 'original/FrostBridge-test.1-esp32-en'

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.package = self.directory / self.id().split('.')[-1]
        shutil.copytree(self.original, self.package)

    def cli(self, *arguments):
        return subprocess.run([sys.executable, str(self.package / 'frostbridge.py'), 'install',
                               '--port', 'NO-USB-OPENED', '--dry-run', *arguments],
                              capture_output=True, text=True)

    def test_initial_install_uses_separate_images_and_preserves_nvs_gap(self):
        result = self.cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        command = json.loads(result.stdout)['command']
        self.assertEqual([value for value in command if value.startswith('0x')],
                         ['0x1000', '0x8000', '0xe000', '0x10000'])
        self.assertNotIn('--erase-all', command)
        self.assertNotIn('0x9000', command)

    def test_update_writes_only_application_after_partition_check(self):
        result = self.cli('--update')
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertTrue(plan['partition_check'])
        self.assertEqual([value for value in plan['command'] if value.startswith('0x')], ['0x10000'])

    def test_chinese_bundle_defaults_to_chinese_and_accepts_english_override(self):
        path = self.package / 'firmware/manifest.json'
        data = json.loads(path.read_text()); data['homekit_language'] = 'zh-CN'
        path.write_text(json.dumps(data))
        command = [sys.executable, str(self.package / 'frostbridge.py')]
        result = subprocess.run(command + ['pair', '--port', 'NO-USB-OPENED'], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('请先运行', result.stderr)
        result = subprocess.run(command + ['--lang', 'en', 'pair', '--port', 'NO-USB-OPENED'], capture_output=True, text=True)
        self.assertIn('Run: python frostbridge.py setup', result.stderr)

    def test_update_does_not_write_when_connected_partition_differs(self):
        # External esptool adapter, not internal installer mocks. No serial is opened.
        adapter = self.directory / 'fake-esptool'
        adapter.mkdir(exist_ok=True)
        (adapter / 'esptool.py').write_text('''import pathlib,sys
with pathlib.Path(__file__).with_suffix('.calls').open('a') as f:f.write(' '.join(sys.argv[1:])+'\\n')
if 'read-flash' in sys.argv:pathlib.Path(sys.argv[-1]).write_bytes(b'not-the-required-partitions')
''')
        env = os.environ.copy(); env['PYTHONPATH'] = str(adapter)
        result = subprocess.run([str(ROOT / '.venv/bin/python') if os.name != 'nt' else str(ROOT / '.venv/Scripts/python.exe'),
                                 str(ROOT / 'frostbridge.py'), 'install', '--package', str(self.package),
                                 '--port', 'NO-USB-OPENED', '--update'], env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Existing partition layout differs', result.stderr)
        calls = (adapter / 'esptool.calls').read_text()
        self.assertIn('read-flash', calls)
        self.assertNotIn('write-flash', calls)

    def test_update_reenters_bootloader_after_partition_read(self):
        # The real ESP32 reported an invalid crystal clock when the next process
        # reused a running read-flash stub. Model that external driver behavior.
        adapter = self.directory / 'reset-required-esptool'
        adapter.mkdir()
        (adapter / 'esptool.py').write_text('''import os,pathlib,sys
if 'read-flash' in sys.argv:
 pathlib.Path(sys.argv[-1]).write_bytes(pathlib.Path(os.environ['FROST_PARTITION_FIXTURE']).read_bytes())
elif 'write-flash' in sys.argv:
 if sys.argv[sys.argv.index('--before')+1]!='default-reset':
  print('Flasher stub must be reset before the new write session',file=sys.stderr);sys.exit(2)
 print('Hash of data verified.')
''')
        env = os.environ.copy(); env['PYTHONPATH'] = str(adapter)
        env['FROST_PARTITION_FIXTURE'] = str(self.package / 'firmware/partitions.bin')
        python = ROOT / ('.venv/Scripts/python.exe' if os.name == 'nt' else '.venv/bin/python')
        result = subprocess.run([str(python), str(ROOT / 'frostbridge.py'), 'install',
                                 '--package', str(self.package), '--port', 'NO-USB-OPENED', '--update'],
                                env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Hash of data verified.', result.stdout)

    def test_corrupted_firmware_is_rejected_before_usb(self):
        path = self.package / 'firmware/application.bin'
        image = bytearray(path.read_bytes()); image[-1] ^= 1; path.write_bytes(image)
        result = self.cli()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('checksum mismatch', result.stderr)

    def test_manifest_cannot_redirect_writes_to_nvs(self):
        path = self.package / 'firmware/manifest.json'
        data = json.loads(path.read_text())
        data['parts'][-1]['offset'] = 0x9000
        path.write_text(json.dumps(data))
        result = self.cli()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unexpected flash part', result.stderr)

    def test_unknown_partition_layout_is_rejected_even_with_matching_checksum(self):
        image = self.package / 'firmware/partitions.bin'
        value = bytearray(image.read_bytes()); value[8] ^= 1; image.write_bytes(value)
        path = self.package / 'firmware/manifest.json'
        data = json.loads(path.read_text())
        data['parts'][1]['sha256'] = hashlib.sha256(value).hexdigest()
        path.write_text(json.dumps(data))
        result = self.cli()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('huge_app layout', result.stderr)

    def test_package_contains_no_build_toolchain_and_all_checksums_match(self):
        requirements = (self.package / 'requirements-install.txt').read_text()
        self.assertIn('esptool==5.6.0', requirements)
        self.assertNotIn('platformio==', requirements)
        self.assertFalse((self.package / 'platformio.ini').exists())
        self.assertFalse((self.package / '.venv').exists())
        for row in (self.package / 'SHA256SUMS.txt').read_text().splitlines():
            checksum, name = row.split('  ', 1)
            self.assertEqual(hashlib.sha256((self.package / name).read_bytes()).hexdigest(), checksum)
        with zipfile.ZipFile(self.package / 'FrostBridge-source.zip') as archive:
            self.assertIn('FrostBridge/platformio.ini', archive.namelist())
            self.assertFalse(any('/.git/' in path or '/.local/' in path for path in archive.namelist()))


if __name__ == '__main__':
    unittest.main(verbosity=2)

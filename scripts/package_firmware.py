"""Create an installation ZIP from an existing production build."""
import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile
import zipfile

from scripts.firmware_bundle import load_bundle, sha256

ROOT = Path(__file__).resolve().parent.parent


def package(args, export_source):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,79}', args.version):
        raise RuntimeError('Version must be a filename-safe label, e.g. 0.1.0')
    output = Path(args.output).resolve()
    if output.exists():
        raise RuntimeError('Output already exists; choose a new package name.')
    environment = 'gateway_zh' if args.homekit_language == 'zh-CN' else 'gateway'
    build = ROOT / '.pio/build' / environment
    images = {'bootloader': (0x1000, build / 'bootloader.bin'),
              'partitions': (0x8000, build / 'partitions.bin'),
              'boot_app0': (0xe000, ROOT / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'),
              'application': (0x10000, build / 'firmware.bin')}
    if any(not path.is_file() for _, path in images.values()):
        raise RuntimeError('Run the production build for ' + environment + ' before packaging.')
    # A package must not silently combine an older build with newer source.
    build_time = images['application'][1].stat().st_mtime
    build_inputs = [ROOT / 'platformio.ini', *ROOT.glob('src/**/*.cpp'), *ROOT.glob('include/**/*.h'),
                    *ROOT.glob('web/*'), ROOT / 'scripts/embed_web.py', ROOT / 'scripts/pio_assets.py']
    if any(path.is_file() and path.stat().st_mtime > build_time for path in build_inputs):
        raise RuntimeError('Firmware is older than build inputs. Rebuild before packaging.')
    name = 'FrostBridge-' + args.version + '-esp32-' + args.homekit_language
    with tempfile.TemporaryDirectory(prefix='frostbridge-package-') as temp:
        directory = Path(temp) / name
        (directory / 'firmware').mkdir(parents=True)
        (directory / 'scripts').mkdir()
        parts = []
        for role, (offset, source) in images.items():
            target = directory / 'firmware' / (role + '.bin')
            shutil.copy2(source, target)
            parts.append({'role': role, 'offset': offset, 'file': target.relative_to(directory).as_posix(),
                          'size': target.stat().st_size, 'sha256': sha256(target)})
        source = directory / 'FrostBridge-source.zip'
        export_source(argparse.Namespace(output=source))
        channel = 'release' if getattr(args, 'release', False) else 'prerelease' if getattr(args, 'prerelease', False) else 'local-test'
        manifest = {'format': 1, 'version': args.version, 'channel': channel,
                    'chip': 'esp32', 'board': 'esp32dev', 'flash_size': '4MB', 'flash_mode': 'dio',
                    'flash_freq': '40m', 'partition_layout': 'huge_app',
                    'homekit_language': args.homekit_language, 'environment': environment,
                    'source_sha256': sha256(source), 'parts': parts}
        (directory / 'firmware/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        for path in ['frostbridge.py', 'requirements-install.txt', 'LICENSE', 'SECURITY.md', 'THIRD_PARTY_NOTICES.md',
                     'CONTRIBUTING.md', 'CONTRIBUTING.zh-CN.md', 'src/protocol/AlpicoolProtocol.cpp',
                     'test/fixtures/README.md', 'scripts/firmware_bundle.py']:
            (directory / path).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / path, directory / path)
        public_paths = [line.strip() for line in (ROOT / 'public-files.txt').read_text().splitlines()
                        if line.strip() and not line.startswith('#')]
        for path in public_paths:
            if path.startswith(('docs/en/', 'docs/zh-CN/', 'third_party/')):
                (directory / path).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / path, directory / path)
        for language, filename in [('en', 'README.md'), ('zh-CN', 'README.zh-CN.md')]:
            guide = (ROOT / 'docs' / language / 'prebuilt.md').read_text(encoding='utf-8')
            nav = '[English](README.md) · [简体中文](README.zh-CN.md)'
            guide = re.sub(r'^\[English\].*$', nav, guide, count=1, flags=re.MULTILINE)
            for link in ['compatibility.md', 'usage.md', 'troubleshooting.md']:
                guide = guide.replace('](' + link + ')', '](docs/' + language + '/' + link + ')')
            (directory / filename).write_text(guide, encoding='utf-8')
        # Check exactly what the installer will consume before sealing the ZIP.
        load_bundle(directory)
        sums = ''.join(sha256(path) + '  ' + path.relative_to(directory).as_posix() + '\n'
                       for path in sorted(directory.rglob('*')) if path.is_file())
        (directory / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open('xb') as file:
            with zipfile.ZipFile(file, 'w', zipfile.ZIP_DEFLATED) as archive:
                for path in sorted(directory.rglob('*')):
                    if path.is_file():
                        archive.write(path, path.relative_to(directory.parent))
    print('Installation package:', output)
    print('SHA256:', sha256(output))

#!/usr/bin/env python3
"""Package corresponding library sources and build information for a release."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_sources(output, upstream, version):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,79}', version):
        raise RuntimeError('Version must be a filename-safe label')
    if output.exists():
        raise RuntimeError('Output already exists')
    required = ['esp-idf-v5.5.5.tar.gz', 'arduino-lib-builder-6671d0b.tar.gz',
                'esp-protocols-mdns-v1.13.1.tar.gz', 'sdk-materials.json',
                'mbedtls-9d669eadb195.tar.gz', 'esp-lwip-fd432e4ee2cf.tar.gz', 'tlsf-2867f6883a12.tar.gz']
    if any(not (upstream / name).is_file() for name in required):
        raise RuntimeError('Missing pinned SDK source material')
    with tempfile.TemporaryDirectory(prefix='frostbridge-sources-') as temp:
        stage = Path(temp) / ('FrostBridge-' + version + '-source-support')
        stage.mkdir()
        subprocess.run([__import__('sys').executable, str(ROOT / 'frostbridge.py'), 'export',
                        '--output', str(stage / 'FrostBridge-source.zip')], check=True)
        # Actual source packages used by the build. Bundled vendor executables are
        # not source and are not redistributed in this archive.
        dependencies = {
            'arduino-esp32': ROOT / '.platformio/packages/framework-arduinoespressif32',
            'HomeSpan': ROOT / '.pio/libdeps/gateway/HomeSpan',
            'NimBLE-Arduino': ROOT / '.pio/libdeps/gateway/NimBLE-Arduino',
            'ArduinoJson': ROOT / '.pio/libdeps/gateway/ArduinoJson',
        }
        for name, source in dependencies.items():
            target = stage / 'libraries' / name
            for path in sorted(source.rglob('*')):
                if not path.is_file() or path.is_symlink():
                    continue
                relative = path.relative_to(source)
                if any(x in ('.git', '__pycache__') for x in relative.parts):
                    continue
                if path.name in ('.piopm', 'installed.json') or path.suffix.lower() in ('.exe', '.elf', '.a', '.so', '.dylib', '.dll', '.pyc'):
                    continue
                destination = target / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, destination)
        shutil.copy2(ROOT / 'third_party/licenses/Arduino-ESP32-LGPL-2.1.md', stage / 'libraries/arduino-esp32/LICENSE.md')
        shutil.copytree(ROOT / 'third_party', stage / 'third_party')
        sdk = ROOT / '.platformio/packages/framework-arduinoespressif32-esp32-libs'
        for name in ['versions.txt', 'sdkconfig', 'package.json']:
            (stage / 'sdk-build').mkdir(exist_ok=True)
            shutil.copy2(sdk / name, stage / 'sdk-build' / name)
        shutil.copytree(sdk / 'flags', stage / 'sdk-build/flags')
        (stage / 'sdk-sources').mkdir()
        for source in sorted(upstream.iterdir()):
            if source.is_file() and (source.name.endswith('.tar.gz') or source.name == 'sdk-materials.json'):
                shutil.copy2(source, stage / 'sdk-sources' / source.name)
        compiler = ROOT / '.platformio/packages/toolchain-xtensa-esp-elf'
        for folder in ['gcc', 'picolibc']:
            shutil.copytree(compiler / 'share/licenses' / folder, stage / 'toolchain-runtime-notices' / folder)
        packages = {}
        for name in ['framework-arduinoespressif32', 'framework-arduinoespressif32-esp32-libs', 'toolchain-xtensa-esp-elf', 'tool-esptool']:
            data = json.loads((ROOT / '.platformio/packages' / name / 'package.json').read_text(encoding='utf-8'))
            packages[name] = data['version']
        images = {}
        for environment in ['gateway', 'gateway_zh']:
            image = ROOT / '.pio/build' / environment / 'firmware.bin'
            images[environment] = {'bytes': image.stat().st_size, 'sha256': digest(image)}
        info = {'version': version, 'platform': 'espressif32 8.0.0', 'packages': packages,
                'images': images, 'library_modifications': 'None; PlatformIO package adaptations retained as received',
                'project_build_flags': 'See platformio.ini and scripts/pio_assets.py in FrostBridge-source.zip',
                'source_archives': {x.name: digest(x) for x in (stage / 'sdk-sources').glob('*.tar.gz')}}
        (stage / 'BUILD-INFO.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
        for language, filename in [('en', 'README.md'), ('zh-CN', 'README.zh-CN.md')]:
            text = (ROOT / 'docs' / language / 'rebuilding.md').read_text(encoding='utf-8')
            text = re.sub(r'^\[English\].*$', '[English](README.md) · [简体中文](README.zh-CN.md)', text, count=1, flags=re.MULTILINE)
            (stage / filename).write_text(text, encoding='utf-8')
        sums = ''.join(digest(p) + '  ' + p.relative_to(stage).as_posix() + '\n' for p in sorted(stage.rglob('*')) if p.is_file())
        (stage / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')
        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob('*')):
                if path.is_file():
                    archive.write(path, path.relative_to(stage.parent))
    print('Source support archive:', output, digest(output))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    package_sources(args.output, args.upstream, args.version)

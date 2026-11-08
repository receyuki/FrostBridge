"""Prebuilt classic ESP32 installation. No build tools or serial imports at load time."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_bundle(directory):
    directory = Path(directory).resolve()
    try:
        data = json.loads((directory / 'firmware' / 'manifest.json').read_text(encoding='utf-8'))
        if (data['format'] != 1 or data['chip'] != 'esp32' or data['flash_size'] != '4MB'
                or data['partition_layout'] != 'huge_app' or data['flash_mode'] != 'dio'
                or data['flash_freq'] != '40m' or data['homekit_language'] not in ('en', 'zh-CN')):
            raise ValueError('Unsupported firmware configuration')
        expected = {'bootloader': (0x1000, 0x7000), 'partitions': (0x8000, 0x1000),
                    'boot_app0': (0xe000, 0x2000), 'application': (0x10000, 0x300000)}
        parts = {}
        for part in data['parts']:
            role = part['role']
            offset, maximum = expected[role]
            path = directory / 'firmware' / (role + '.bin')
            if role in parts or part['file'] != 'firmware/' + role + '.bin' or part['offset'] != offset:
                raise ValueError('Unexpected flash part')
            if path.is_symlink() or not path.resolve().is_relative_to(directory):
                raise ValueError('Firmware must be inside the package')
            if not 0 < path.stat().st_size <= maximum or path.stat().st_size != part['size']:
                raise ValueError('Invalid image size: ' + role)
            if sha256(path) != part['sha256']:
                raise ValueError('Firmware checksum mismatch: ' + role)
            parts[role] = (offset, path)
        if parts.keys() != expected.keys():
            raise ValueError('Incomplete firmware package')
        # Fixed huge_app layout: in particular, NVS stays at 0x9000..0xdfff.
        import struct
        entries = []
        table = parts['partitions'][1].read_bytes()
        for pos in range(0, len(table) - 31, 32):
            magic, kind, subtype, offset, size, label, flags = struct.unpack('<HBBII16sI', table[pos:pos+32])
            if magic != 0x50aa:
                break
            entries.append((kind, subtype, offset, size))
        if entries != [(1, 2, 0x9000, 0x5000), (1, 0, 0xe000, 0x2000),
                       (0, 16, 0x10000, 0x300000), (1, 130, 0x310000, 0xe0000),
                       (1, 3, 0x3f0000, 0x10000)]:
            raise ValueError('Partition table is not the supported huge_app layout')
        return data, parts
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise RuntimeError('Invalid prebuilt package: ' + str(error)) from error


def install(directory, port, python, update=False, dry_run=False, translate=lambda en, zh: en):
    data, parts = load_bundle(directory)
    base = [str(python), '-m', 'esptool', '--chip', 'esp32', '--port', port, '--baud', '115200']
    # A second esptool process must start from ROM, not reuse the read-flash
    # stub's UART/clock state (which can mis-detect the ESP32 crystal clock).
    write = base + ['--before', 'default-reset', '--after', 'hard-reset',
                    'write-flash', '--flash-mode', 'dio', '--flash-freq', '40m', '--flash-size', '4MB']
    for role in (['application'] if update else ['bootloader', 'partitions', 'boot_app0', 'application']):
        offset, path = parts[role]
        write += [hex(offset), str(path)]
    partition = parts['partitions'][1]
    if dry_run:
        print(json.dumps({'version': data['version'], 'homekit_language': data['homekit_language'],
                          'mode': 'update' if update else 'install',
                          'partition_check': update, 'command': write}, indent=2))
        return
    print(translate('Installing prebuilt firmware:', '正在安装预编译固件：'), data['version'], '(' + data['homekit_language'] + ')', flush=True)
    if update:
        print(translate('Checking the existing partition layout before updating.', '正在核对设备现有分区布局。'), flush=True)
        with tempfile.TemporaryDirectory(prefix='frostbridge-partition-') as temp:
            actual = Path(temp) / 'partitions.bin'
            subprocess.run(base + ['--after', 'no-reset', 'read-flash', '0x8000',
                                  str(partition.stat().st_size), str(actual)], check=True)
            if actual.read_bytes() != partition.read_bytes():
                raise RuntimeError(translate('Existing partition layout differs; no update written. Use initial-install instructions for a dedicated board.', '现有分区布局不同，未写入更新。请按专用开发板的首次安装说明操作。'))
    print(translate('Writing firmware over USB; wait for device verification.', '正在通过USB写入固件，请等待设备校验完成。'), flush=True)
    subprocess.run(write, check=True)
    print(translate('USB write verified. First installation: run wifi, then bind, then pair. For an update, keep configurations that already work.', 'USB写入校验通过。首次安装请依次运行wifi、bind、pair；升级时保留已有的正常配置，无需重配。'))

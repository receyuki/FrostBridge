#!/usr/bin/env python3
"""FrostBridge: build, flash and configure from a normal terminal (Python 3.10+)."""
from __future__ import annotations
import argparse
import getpass
import html
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
import venv
import warnings
import webbrowser
import zipfile

ROOT = Path(__file__).resolve().parent
PYTHON = ROOT / '.venv' / ('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
LANG = 'en'

def text(en, zh):
    return zh if LANG == 'zh-CN' else en

def run(command):
    environment = os.environ.copy()
    environment['PLATFORMIO_CORE_DIR'] = str(ROOT / '.platformio')
    return subprocess.run([str(c) for c in command], cwd=ROOT, env=environment, check=True)

def require_tools():
    if not PYTHON.exists():
        raise RuntimeError(text('Run: python frostbridge.py setup', '请先运行：python frostbridge.py setup'))

def use_venv():
    require_tools()
    if Path(sys.prefix).resolve() != (ROOT / '.venv').resolve():
        raise SystemExit(subprocess.call([str(PYTHON), str(Path(__file__).resolve()), *sys.argv[1:]], cwd=ROOT))

def pio(*arguments):
    require_tools()
    return run([PYTHON, '-m', 'platformio', *arguments])

class USB:
    """One session per operation; scan and selection never cross a reboot."""
    def __init__(self, port):
        import serial
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=.25)
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = port
    def __enter__(self):
        print(text('Opening USB may restart ESP32. Stop other serial monitors first.', '打开USB可能重启ESP32；请先关闭其他串口监视器。'))
        try:
            self.serial.open()
            self.read(6)
        except BaseException:
            self.serial.close()
            raise
        return self
    def __exit__(self, *unused):
        self.serial.close()
    def read(self, seconds, prefix=None, stop=None):
        rows = []
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            line = self.serial.readline().decode('utf-8', errors='replace').strip()
            if not line:
                continue
            if prefix is None or line.startswith(prefix):
                rows.append(line)
            if stop and stop(line):
                break
        return rows
    def command(self, command, prefix, seconds=10, stop=None):
        self.serial.write((command + '\n').encode('utf-8'))
        self.serial.flush()
        return self.read(seconds, prefix, stop)

def wifi(args):
    if not sys.stdin.isatty():
        raise RuntimeError(text('Wi-Fi setup requires an interactive terminal; passwords are never command arguments.', '配网需要交互式终端，密码不接受命令行参数。'))
    ssid = input(text('2.4 GHz Wi-Fi SSID: ', '2.4GHz Wi-Fi名称：'))
    with warnings.catch_warnings():
        warnings.simplefilter('error', getpass.GetPassWarning)
        try:
            password = '' if args.keep_password else getpass.getpass(text('Wi-Fi password (hidden): ', 'Wi-Fi密码（隐藏输入）：'))
        except getpass.GetPassWarning:
            raise RuntimeError(text('This terminal cannot hide input. Use an interactive terminal with password masking.', '终端不能隐藏输入，请使用支持密码掩码的交互式终端。')) from None
    if not 1 <= len(ssid.encode()) <= 32 or (not args.keep_password and not 8 <= len(password.encode()) <= 63):
        raise RuntimeError(text('SSID: 1–32 UTF-8 bytes; password: 8–63 bytes.', 'SSID需1–32字节，密码需8–63字节。'))
    if any(c in ssid + password for c in '\t\r\n\0'):
        raise RuntimeError(text('Tabs, line breaks and NUL are unsupported.', '不支持制表符、换行或NUL。'))
    with USB(args.port) as usb:
        command = 'SSID\t' + ssid if args.keep_password else 'WIFI\t' + ssid + '\t' + password
        usb.serial.write((command + '\n').encode())
        command = password = ''
        rows = usb.read(18, 'M0_CONFIG_', lambda line: line.startswith('M0_CONFIG_'))
        if not rows or rows[-1] != 'M0_CONFIG_SAVED':
            raise RuntimeError(text('Configuration was not confirmed; check port and firmware. Credentials were not logged.', '配置未确认，请检查串口和固件；凭据未记录。'))
    print(text('Saved on ESP32; restarting. No credentials saved on this computer.', '已保存至ESP32并重启；电脑未保存凭据。'))

def bind(args):
    with USB(args.port) as usb:
        info = usb.command('BLE_STATUS', 'FROST_BLE_STATUS', 3, lambda line: line.startswith('FROST_BLE_STATUS'))
        if not info or not re.fullmatch(r'FROST_BLE_STATUS selected=[01] connected=[01] fresh=[01]', info[-1]):
            raise RuntimeError(text('Binding status is unknown. Update the gateway firmware before selecting a fridge; existing selections were not changed.', '无法确认已有绑定，请先更新gateway固件再选择冰箱；未更改既有目标。'))
        selected = 'selected=1' in info[-1]
        if selected and not args.replace:
            print(text('A fridge is already selected; keeping it. Use --replace only to select another fridge.', '已有目标冰箱，保留现有选择；只有要更换冰箱时才使用--replace。'))
            return
        if selected:
            usb.serial.write(b'BLE_DISCONNECT\n')
        print(text('Scanning for 12 seconds. Disconnect the factory app/nRF Connect first.', '扫描12秒；请先退出原厂App/nRF Connect。'))
        usb.read(12)
        rows = usb.command('BLE_LIST', 'M1_DEVICE ', 5, lambda line: line == 'M1_LIST_END')
        choices = {}
        for line in rows:
            match = re.fullmatch(r'M1_DEVICE index=(\d+) address=(\S+) type=(\d+) rssi=(-?\d+) service1234=(\d+) name_hex=([0-9A-Fa-f]*)', line)
            if not match:
                continue
            index, address, kind, rssi, service, name = match.groups()
            name = bytes.fromhex(name).decode('utf-8', errors='replace') or '(unnamed)'
            name = ''.join(c if c.isprintable() else '?' for c in name)
            choices[int(index)] = line
            print(f'{index}: {name}  {address}  RSSI {rssi}  service1234={service}')
        if not choices:
            raise RuntimeError(text('No scan candidates. Check power/range and other BLE clients; existing selections are not cleared.', '未找到候选，请检查供电、距离和其他蓝牙客户端；没有清除既有绑定。'))
        answer = input(text('Index of YOUR fridge (blank cancels): ', '选择你自己的冰箱序号（留空取消）：')).strip()
        if not answer:
            return
        if not answer.isdigit() or int(answer) not in choices:
            raise RuntimeError(text('Choose one of the indices shown in this session.', '请选择本次会话显示的序号。'))
        usb.serial.write(f'BLE_TARGET\t{int(answer)}\n'.encode())
        rows = usb.read(40, stop=lambda line: line.startswith(('M1_STATE ', 'M1_TARGET_REJECTED', 'M1_UNSUPPORTED')))
        if not any(line.startswith('M1_STATE ') for line in rows):
            raise RuntimeError(text('Fresh fridge state was not confirmed. Check the device and retry; no cooling setting was requested.', '未确认新鲜状态，请检查设备后重试；未请求改变制冷设置。'))
        if 'FROST_BIND_SAVED' not in rows:
            raise RuntimeError(text('Fresh state received, but saving the target was not confirmed. Check USB diagnostics before relying on reboot recovery.', '已读到新鲜状态，但保存目标未确认，请检查USB诊断后再依赖重启恢复。'))
        print(text('Fresh state received; selected fridge saved locally.', '已收到新鲜状态，所选冰箱已保存至ESP32。'))

def pair(args):
    with USB(args.port) as usb:
        rows = usb.command('PAIR', 'FROST_PAIR ', 5, lambda line: line.startswith('FROST_PAIR '))
        if not rows:
            raise RuntimeError(text('No production pairing code. Flash the gateway environment first.', '没有生产配对码，请先刷写gateway环境。'))
        fields = rows[-1].split()
        if len(fields) != 3 or not re.fullmatch(r'\d{8}', fields[1]) or not re.fullmatch(r'X-HM://[0-9A-Z]+', fields[2]):
            raise RuntimeError(text('Unexpected pairing response.', '配对响应格式异常。'))
        code = fields[1]
        print(text('Private setup code — do not post in issues:', '私人设置代码，请勿贴到公开Issue：'))
        print(f'{code[:3]}-{code[3:5]}-{code[5:]}')
        print(fields[2])
        import qrcode
        from qrcode.image.svg import SvgPathImage
        qr=qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M,box_size=8,border=4)
        qr.add_data(fields[2]);qr.make(fit=True)
        svg=qr.make_image(image_factory=SvgPathImage).to_string(encoding='unicode')
        directory=ROOT/'.local'/'pairing';directory.mkdir(parents=True,exist_ok=True,mode=0o700)
        output=directory/('pairing-'+secrets.token_hex(4)+'.html')
        title=text('Add FrostBridge to Apple Home','将FrostBridge添加到Apple家庭')
        instructions=text('On your iPhone/iPad: Home → + → Add Accessory. Scan this code on the computer screen.','在iPhone/iPad上打开家庭→＋→添加配件，扫描电脑屏幕上的二维码。')
        document='<!doctype html><html lang="'+LANG+'"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><meta http-equiv="Content-Security-Policy" content="default-src \'none\'; style-src \'unsafe-inline\'"><title>FrostBridge pairing</title><style>body{font:18px system-ui;max-width:540px;margin:40px auto;padding:20px;text-align:center}svg{width:300px;max-width:100%;height:auto;background:white}strong{display:block;font-size:32px;letter-spacing:3px;margin:20px}p{line-height:1.6}</style><h1>'+html.escape(title)+'</h1><p>'+html.escape(instructions)+'</p>'+svg+'<strong>'+code[:3]+'-'+code[3:5]+'-'+code[5:]+'</strong><p>'+html.escape(text('Private local page. Keep this code secret. No network requests are made.','私人本地页面，请保管好设置码；此页面不发起网络请求。'))+'</p></html>'
        descriptor=os.open(output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(descriptor,'w',encoding='utf-8') as stream:stream.write(document)
        print(text('Local QR page: ','本地二维码页：')+str(output))
        if not args.no_open:webbrowser.open(output.as_uri())
        print(text('Apple Home → Add Accessory → More Options → FrostBridge → enter the code. Accept the uncertified-accessory prompt if shown.', '家庭App→添加配件→更多选项→FrostBridge→输入代码；如出现未经认证提示，确认继续。'))

def status(args):
    host = args.host
    if not re.fullmatch(r'[A-Za-z0-9.-]+', host):
        raise RuntimeError('Use a local hostname or IPv4 address, without http:// or a port.')
    with urllib.request.urlopen('http://' + host + '/api/v1/status', timeout=6) as response:
        data = json.load(response)
    print(json.dumps(data, ensure_ascii=False, indent=2))

def source_export(args):
    """Allowlisted source only; no Git history, build cache or development records."""
    manifest=ROOT/'public-files.txt'
    if not manifest.is_file():
        raise RuntimeError('Missing public-files.txt; source export requires a reviewed manifest.')
    paths=[];seen=set()
    for row in manifest.read_text(encoding='utf-8').splitlines():
        name=row.strip()
        if not name or name.startswith('#'):
            continue
        relative=Path(name)
        if '\\' in name or relative.name.startswith(('.env','secrets.')) or relative.name=='secrets.h':
            raise RuntimeError('Private/nonportable public manifest entry: '+name)
        if relative.is_absolute() or '..' in relative.parts or any(part in {'.git','.local','.scratch','.venv','.pio','.platformio','__pycache__'} for part in relative.parts) or name in seen:
            raise RuntimeError('Unsafe/duplicate public manifest entry: '+name)
        seen.add(name);path=ROOT/relative
        if not path.is_file() or any(parent.is_symlink() for parent in (path,*path.parents) if parent!=ROOT.parent):
            raise RuntimeError('Missing or symlinked public source: '+name)
        paths.append(path)
    output = Path(args.output).resolve()
    if output.exists():
        raise RuntimeError(text('Output already exists; choose a new name to preserve it.', '输出已存在，请使用新名称，保留原文件。'))
    for path in paths:
        if path.is_symlink() or not path.resolve().is_relative_to(ROOT):
            raise RuntimeError('Refuse symlink or external source: ' + str(path.relative_to(ROOT)))
        if path.suffix in ('.md', '.py', '.cpp', '.h', '.yml', '.ini'):
            content = path.read_text(encoding='utf-8')
            private_prefixes = ('/' + 'Users' + '/', 'codex' + ':/' + '/', 'chatgpt' + '.com/share/')
            if any(prefix in content for prefix in private_prefixes) or re.search(r'(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}', content):
                raise RuntimeError('Review private/machine-specific content before export: ' + str(path.relative_to(ROOT)))
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(paths):
            archive.write(path, Path('FrostBridge') / path.relative_to(ROOT))
    print(text('Source-only archive created: ', '已生成不含Git历史的源码包：') + str(output))

def main():
    global LANG
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lang', choices=['en', 'zh-CN'], default='en', help='Terminal messages')
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('setup', help='Create .venv and install pinned build tools')
    for command in ('build', 'flash'):
        item = commands.add_parser(command, help='Build' if command == 'build' else 'Build and flash')
        item.add_argument('--homekit-language', choices=['en', 'zh-CN'], default='en', help='Factory accessory names (default: en); Web has its own selector')
        if command == 'flash':
            item.add_argument('--port', required=True)
    commands.add_parser('ports', help='List USB ports')
    for name in ('wifi', 'bind', 'pair', 'monitor'):
        item = commands.add_parser(name)
        item.add_argument('--port', required=True)
        if name == 'wifi':
            item.add_argument('--keep-password', action='store_true', help='Change SSID only')
        if name == 'pair':
            item.add_argument('--no-open', action='store_true', help='Generate local QR page without opening the browser')
        if name == 'bind':
            item.add_argument('--replace', action='store_true', help='Explicitly select another fridge')
    item = commands.add_parser('status', help='Read-only local HTTP status')
    item.add_argument('--host', default='frostbridge.local')
    commands.add_parser('doctor', help='Check local tools without opening USB')
    commands.add_parser('test', help='Run host C++ tests (clang++)')
    item = commands.add_parser('export', help='Create clean source ZIP, excluding Git history')
    item.add_argument('--output', default=str(ROOT / '.local/dist/FrostBridge-source.zip'))
    args = parser.parse_args()
    LANG = args.lang
    if sys.version_info < (3, 10):
        raise RuntimeError('Python 3.10+ is required.')
    if args.command == 'setup':
        if not PYTHON.exists():
            venv.EnvBuilder(with_pip=True).create(ROOT / '.venv')
        run([PYTHON, '-m', 'pip', 'install', '--disable-pip-version-check', '-r', ROOT / 'requirements.txt'])
        print(text('Ready. Next: python frostbridge.py build', '工具就绪。下一步：python frostbridge.py build'))
    elif args.command in ('build', 'flash'):
        environment = 'gateway_zh' if args.homekit_language == 'zh-CN' else 'gateway'
        arguments = ['run', '-e', environment]
        if args.command == 'flash':
            arguments += ['-t', 'upload', '--upload-port', args.port]
        pio(*arguments)
    elif args.command in ('ports', 'wifi', 'bind', 'pair', 'monitor'):
        use_venv()
        if args.command == 'ports':
            from serial.tools import list_ports
            for port in list_ports.comports():
                print(port.device + '  ' + port.description)
        elif args.command == 'monitor':
            print(text('Ctrl+] exits; logs can contain private device data. Opening USB may restart ESP32.', 'Ctrl+]退出；日志可能含私人设备信息，打开USB可能重启ESP32。'))
            run([PYTHON, '-m', 'serial.tools.miniterm', '--dtr', '0', '--rts', '0', args.port, '115200'])
        else:
            {'wifi': wifi, 'bind': bind, 'pair': pair}[args.command](args)
    elif args.command == 'status':
        status(args)
    elif args.command == 'export':
        source_export(args)
    elif args.command == 'doctor':
        print('Python:', sys.version.split()[0])
        print('Project:', ROOT)
        print('Project virtualenv:', 'OK' if PYTHON.exists() else 'missing: run setup')
        print('clang++:', shutil.which('clang++') or 'not installed (only needed for host tests)')
        if PYTHON.exists():
            run([PYTHON, '-m', 'platformio', '--version'])
    elif args.command == 'test':
        require_tools()
        if not shutil.which('clang++'):
            raise RuntimeError('Host tests need clang++ with ASan/UBSan; macOS is locally validated and Linux CI is configured.')
        for name in ['protocol', 'controller', 'schedule', 'trusted_clock', 'daily_replay', 'runtime', 'http', 'mailbox', 'portal', 'homekit', 'homekit_bridge']:
            run([PYTHON, ROOT / 'scripts' / ('test_' + name + '.py')])

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit(130)
    except (RuntimeError, OSError, subprocess.CalledProcessError, urllib.error.URLError) as error:
        print(text('Error: ', '错误：') + str(error), file=sys.stderr)
        raise SystemExit(1)

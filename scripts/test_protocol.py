#!/usr/bin/env python3
"""Run protocol behavior tests on the host, independent of Arduino/BLE."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
output = root / '.local/tests/protocol'
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
                '-fsanitize=address,undefined', '-I'+str(root/'include'),
                str(root/'test/test_protocol.cpp'), str(root/'src/protocol/AlpicoolProtocol.cpp'),
                '-o', str(output)], check=True)
subprocess.run([str(output)], check=True, cwd=root)

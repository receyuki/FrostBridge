#!/usr/bin/env python3
"""Exercise the public control interface with controlled time/device transport."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
output = root / '.local/tests/controller'
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
                '-fsanitize=address,undefined', '-I'+str(root/'include'),
                str(root/'test/test_controller.cpp'),
                str(root/'src/control/FridgeController.cpp'),
                str(root/'src/protocol/AlpicoolProtocol.cpp'), '-o', str(output)], check=True)
subprocess.run([str(output)], check=True, cwd=root)

#!/usr/bin/env python3
"""Test the combined control/plan interface against a simulated device."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
output = root / '.local/tests/runtime'
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
                '-fsanitize=address,undefined', '-I'+str(root/'include'),
                str(root/'test/test_runtime.cpp'),
                *[str(root/p) for p in ['src/control/ControlRuntime.cpp',
                  'src/control/FridgeController.cpp', 'src/control/DailyReplay.cpp',
                  'src/control/TrustedClock.cpp', 'src/protocol/AlpicoolProtocol.cpp']],
                '-o', str(output)], check=True)
subprocess.run([str(output)], check=True, cwd=root)

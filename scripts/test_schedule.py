#!/usr/bin/env python3
"""Test weekly scheduling through the public plan interface."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
output = root / '.local/tests/schedule'
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
                '-fsanitize=address,undefined', '-I'+str(root/'include'),
                str(root/'test/test_schedule.cpp'),
                str(root/'src/control/WeeklySchedule.cpp'),
                str(root/'src/control/ScheduleStorage.cpp'), '-o', str(output)], check=True)
subprocess.run([str(output)], check=True, cwd=root)

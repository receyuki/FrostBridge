#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent;output=root/'.local/tests/daily-replay';output.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(root/'include'),str(root/'test/test_daily_replay.cpp'),str(root/'src/control/DailyReplay.cpp'),str(root/'src/control/DailyStorage.cpp'),str(root/'src/control/DailyJournal.cpp'),'-o',str(output)],check=True)
subprocess.run([str(output)],cwd=root,check=True)

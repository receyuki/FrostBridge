#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent;output=root/'.local/tests/trusted-clock';output.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(root/'include'),str(root/'test/test_trusted_clock.cpp'),str(root/'src/control/TrustedClock.cpp'),'-o',str(output)],check=True)
subprocess.run([str(output)],cwd=root,check=True)

#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent;output=root/'.local/tests/mailbox';output.parent.mkdir(parents=True,exist_ok=True)
sources=['test/test_mailbox.cpp','src/web/PortalMailbox.cpp','src/control/DailyReplay.cpp','src/web/PortalWorker.cpp','src/web/PortalTypes.cpp','src/control/ControlRuntime.cpp','src/control/TrustedClock.cpp','src/control/FridgeController.cpp','src/protocol/AlpicoolProtocol.cpp']
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(root/'include'),*[str(root/p) for p in sources],'-o',str(output)],check=True)
subprocess.run([str(output)],cwd=root,check=True)

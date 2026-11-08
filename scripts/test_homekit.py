#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent;output=root/'.local/tests/homekit';output.parent.mkdir(parents=True,exist_ok=True)
sources=['test/test_homekit.cpp','src/homekit/HomeKitControl.cpp','src/web/PortalMailbox.cpp','src/web/PortalTypes.cpp','src/web/PortalWorker.cpp','src/control/ControlRuntime.cpp','src/control/FridgeController.cpp','src/control/DailyReplay.cpp','src/control/TrustedClock.cpp','src/protocol/AlpicoolProtocol.cpp']
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(root/'include'),'-I'+str(root/'.pio/libdeps/gateway/ArduinoJson/src'),*[str(root/p) for p in sources],'-o',str(output)],check=True)
subprocess.run([str(output)],cwd=root,check=True)

#!/usr/bin/env python3
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
output = root / '.local/tests/portal'
output.parent.mkdir(parents=True, exist_ok=True)
json_include=next((root/'.pio/libdeps'/name/'ArduinoJson/src' for name in ('gateway','gateway_zh','gateway_en') if (root/'.pio/libdeps'/name/'ArduinoJson/src').is_dir()),None)
if json_include is None:raise SystemExit('Build a production profile first to fetch ArduinoJson.')
sources=['test/test_portal.cpp','src/web/PortalMailbox.cpp','src/web/PortalApi.cpp','src/web/PortalTypes.cpp','src/web/PortalWorker.cpp',
         'src/control/ControlRuntime.cpp','src/control/FridgeController.cpp',
         'src/control/DailyReplay.cpp','src/control/TrustedClock.cpp','src/protocol/AlpicoolProtocol.cpp']
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
                '-I'+str(root/'include'),'-I'+str(json_include),
                *[str(root/p) for p in sources],'-o',str(output)],check=True)
subprocess.run([str(output)],check=True,cwd=root)

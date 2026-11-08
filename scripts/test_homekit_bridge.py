#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent;output=root/'.local/tests/homekit-bridge';output.parent.mkdir(parents=True,exist_ok=True)
sources=['test/test_homekit_bridge.cpp','src/homekit/HomeKitBridge.cpp','src/homekit/HomeKitControl.cpp','src/web/PortalMailbox.cpp','src/web/PortalTypes.cpp','src/web/PortalWorker.cpp','src/control/ControlRuntime.cpp','src/control/FridgeController.cpp','src/control/DailyReplay.cpp','src/control/TrustedClock.cpp','src/protocol/AlpicoolProtocol.cpp']
for language in ('en','zh-CN'):
    target=output.with_name(output.name+('-en' if language=='en' else ''))
    defines=['-DFROSTBRIDGE_HOMEKIT_ZH'] if language=='zh-CN' else []
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-DFROSTBRIDGE_GATEWAY',*defines,'-I'+str(root/'test/homekit_stub'),'-I'+str(root/'include'),'-I'+str(root/'.pio/libdeps/gateway/ArduinoJson/src'),*[str(root/p) for p in sources],'-o',str(target)],check=True)
    subprocess.run([str(target)],cwd=root,check=True)
    print('PASS: unchanged HAP callback behavior with '+language+' accessory names',flush=True)

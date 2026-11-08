#!/usr/bin/env python3
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
output = root / '.local/tests/http'
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
                '-fsanitize=address,undefined', '-I'+str(root/'include'),
                str(root/'test/test_http.cpp'),str(root/'src/web/BoundedHttp.cpp'),'-o',str(output)],check=True)
subprocess.run([str(output)],check=True,cwd=root)

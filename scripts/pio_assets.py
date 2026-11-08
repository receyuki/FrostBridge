"""PlatformIO pre-build: deterministic static resources, no Node or frontend install."""
Import("env")
import runpy
from pathlib import Path
runpy.run_path(str(Path(env.subst("$PROJECT_DIR"))/"scripts"/"embed_web.py"),run_name="__main__")

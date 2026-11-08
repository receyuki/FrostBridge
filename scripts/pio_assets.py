"""PlatformIO pre-build: deterministic static resources, no Node or frontend install."""
Import("env")
import runpy
from pathlib import Path
project_dir = str(Path(env.subst("$PROJECT_DIR")).resolve())
# Release images and debug objects must not embed the builder's private path.
env.Append(CCFLAGS=["-ffile-prefix-map=" + project_dir + "=."])
runpy.run_path(str(Path(env.subst("$PROJECT_DIR"))/"scripts"/"embed_web.py"),run_name="__main__")

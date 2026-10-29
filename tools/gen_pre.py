# PlatformIO pre-script for the native and sim builds: they compile lib/okame_core too, so keep sprites.h current.
Import("env")  # noqa: F821
import subprocess
import sys

root = env.subst("$PROJECT_DIR")  # noqa: F821
subprocess.check_call([sys.executable, f"{root}/tools/gen_sprites.py"], cwd=root)

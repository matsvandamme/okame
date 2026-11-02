# PlatformIO pre-script for the native and sim builds: they compile lib/okame_core too, so keep sprites.h current.
Import("env")  # noqa: F821
import subprocess
import sys

root = env.subst("$PROJECT_DIR")  # noqa: F821
subprocess.check_call([sys.executable, f"{root}/tools/gen_sprites.py"], cwd=root)

# One source for the firmware version: the VERSION file ("<name> <number>"). A USB (dev) build is "<name>-dev" with the
# same number, so OTA never replaces it with the release of the same number, and the relay enables the right features.
# tools/release.py sets OKAME_RELEASE=1 to build the real thing.
import os
name, number = open(f"{root}/VERSION").read().split()
if os.environ.get("OKAME_RELEASE") != "1":
    name += "-dev"
env.Append(CPPDEFINES=[("FW_VERSION", number), ("FW_NAME", env.StringifyMacro(name))])  # noqa: F821


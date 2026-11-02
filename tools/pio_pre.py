# PlatformIO pre-build hook: regenerate sprites.h and web_assets.h.
Import("env")  # noqa: F821
import subprocess
import sys

root = env.subst("$PROJECT_DIR")  # noqa: F821
for script in ("tools/gen_sprites.py", "tools/embed_web.py"):
    subprocess.check_call([sys.executable, f"{root}/{script}"], cwd=root)

# One source for the firmware version: the VERSION file ("<name> <number>"). A USB (dev) build is "<name>-dev" with the
# same number, so OTA never replaces it with the release of the same number, and the relay enables the right features.
# tools/release.py sets OKAME_RELEASE=1 to build the real thing.
import os
name, number = open(f"{root}/VERSION").read().split()
if os.environ.get("OKAME_RELEASE") != "1":
    name += "-dev"
env.Append(CPPDEFINES=[("FW_VERSION", number), ("FW_NAME", env.StringifyMacro(name))])  # noqa: F821


# The dongle's port name changes with the hub port it's in; find it by its USB serial number.
DONGLE_SERIAL = "3C:0F:02:D9:6C:F8"
if env.subst("$PIOENV") == "okame":  # noqa: F821
    from serial.tools import list_ports

    for p in list_ports.comports():
        if (p.serial_number or "").upper() == DONGLE_SERIAL:
            env.Replace(UPLOAD_PORT=p.device, MONITOR_PORT=p.device)  # noqa: F821
            print(f"okame dongle on {p.device}")
            break
    else:
        # Never let PlatformIO guess: other ESP32 boards share this Mac.
        env.Replace(UPLOAD_PORT="/dev/null/okame-dongle-not-connected")  # noqa: F821
        print("okame dongle not connected: upload will fail instead of picking another board")

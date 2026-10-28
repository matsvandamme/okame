# PlatformIO pre-build hook: regenerate sprites.h and web_assets.h.
Import("env")  # noqa: F821
import subprocess
import sys

root = env.subst("$PROJECT_DIR")  # noqa: F821
for script in ("tools/gen_sprites.py", "tools/embed_web.py"):
    subprocess.check_call([sys.executable, f"{root}/{script}"], cwd=root)

# The dongle's port name changes with the hub port it's in; find it by its USB serial number.
DONGLE_SERIAL = "3C:0F:02:D9:6C:F8"
if env.subst("$PIOENV") == "okame":  # noqa: F821
    from serial.tools import list_ports

    for p in list_ports.comports():
        if (p.serial_number or "").upper() == DONGLE_SERIAL:
            env.Replace(UPLOAD_PORT=p.device, MONITOR_PORT=p.device)  # noqa: F821
            print(f"okame dongle on {p.device}")
            break

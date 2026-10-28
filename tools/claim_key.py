#!/usr/bin/env python3
"""Give a reflashed egg its relay key back, using a one-time claim token from the relay admin.

    python tools/claim_key.py <token>

Fetches the key from the relay (pinned certificate) and types it into the egg over USB.
The key never appears on screen. Needs pyserial (PlatformIO's own Python has it).
"""
import pathlib
import re
import ssl
import sys
import time
import urllib.request

import serial
from serial.tools import list_ports

RELAY = "https://2.28.47.87:8443/v1/claim/"
DONGLE_SERIAL = "3C:0F:02:D9:6C:F8"

root = pathlib.Path(__file__).resolve().parent.parent
pem = "".join(re.findall(r'"([^"]*)"', (root / "src/relay_cert.h").read_text())).replace("\\n", "\n")
ctx = ssl.create_default_context(cadata=pem)
ctx.check_hostname = False  # the certificate itself is pinned

port = next((p.device for p in list_ports.comports() if (p.serial_number or "").upper() == DONGLE_SERIAL), None)
if not port:
    sys.exit("Okame dongle not found: plug it in (and close any serial monitor).")

try:
    key = urllib.request.urlopen(RELAY + sys.argv[1], context=ctx, timeout=15).read().decode().strip()
except Exception as e:
    sys.exit(f"Claim failed ({e}). The token is single-use and expires; ask for a new one.")
if not re.fullmatch(r"[0-9a-f]{64}", key):
    sys.exit("Relay sent something that isn't a key.")

with serial.Serial(port, 115200, timeout=1) as s:
    time.sleep(0.5)
    s.reset_input_buffer()
    s.write(f"cloudkey {key}\n".encode())
    s.write(b"cloud\nstate\n")
    end = time.time() + 4
    while time.time() < end:
        line = s.readline().decode(errors="replace").strip()
        if line.startswith(("CLOUDKEY", "CLOUD ", "STATE")):
            print(line)

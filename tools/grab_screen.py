#!/usr/bin/env python3
"""Ask the dongle for its current frame over USB serial and save it as a PNG.

usage: grab_screen.py OUT.png [--scale N]
"""
import sys
import time

import serial
from serial.tools import list_ports
from PIL import Image

DONGLE_SERIAL = "3C:0F:02:D9:6C:F8"
BG, INK, GHOST, SHADOW = (0x9E, 0xAA, 0x8A), (0x1E, 0x24, 0x18), (0x92, 0x9E, 0x7F), (0x8A, 0x96, 0x7A)


def port():
    for p in list_ports.comports():
        if (p.serial_number or "").upper() == DONGLE_SERIAL:
            return p.device
    sys.exit("dongle not found")


def grab():
    s = serial.Serial(port(), 115200, timeout=0.2)
    s.reset_input_buffer()
    s.write(b"screen\n")
    end = time.time() + 3
    while time.time() < end:
        line = s.readline().decode(errors="replace").strip()
        if line.startswith("FRAME "):
            return bytes.fromhex(line[6:])
    sys.exit("no frame received")


def render(packed, scale):
    img = Image.new("RGB", (160, 80), BG)
    px = img.load()
    for y in range(20):
        for x in range(40):
            i = y * 40 + x
            on = packed[i // 8] & (0x80 >> (i % 8))
            for dy in range(4):
                for dx in range(4):
                    if dx < 3 and dy < 3:
                        c = INK if on else GHOST
                    else:
                        c = SHADOW if on else BG
                    px[x * 4 + dx, y * 4 + dy] = c
    return img.resize((160 * scale, 80 * scale), Image.NEAREST)


if __name__ == "__main__":
    scale = int(sys.argv[sys.argv.index("--scale") + 1]) if "--scale" in sys.argv else 4
    render(grab(), scale).save(sys.argv[1])
    print(sys.argv[1])

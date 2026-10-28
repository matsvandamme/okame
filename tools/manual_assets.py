#!/usr/bin/env python3
"""Export sprites as crisp PNGs (ink on transparent) for the manual: out/manual_img/spr_<name>.png"""
import pathlib
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from gen_sprites import parse  # noqa: E402

out = ROOT / "manual" / "img"
out.mkdir(parents=True, exist_ok=True)
INK, PAT = (30, 36, 24, 255), (30, 36, 24, 255)
for f in ("bird.txt", "icons.txt"):
    for name, rows in parse(ROOT / "assets" / "sprites" / f):
        s = 24
        img = Image.new("RGBA", (len(rows[0]) * s, len(rows) * s), (0, 0, 0, 0))
        px = img.load()
        for y, r in enumerate(rows):
            for x, ch in enumerate(r):
                on = ch == "#" or (ch == "w" and (x + y) % 2 == 0)
                if on:
                    for dy in range(s - 3):
                        for dx in range(s - 3):
                            px[x * s + dx, y * s + dy] = INK
        img.save(out / f"spr_{name}.png")
print("sprites exported")

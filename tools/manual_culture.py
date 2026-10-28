#!/usr/bin/env python3
"""Render the booklet-only colour pixel art in assets/manual/*.txt to manual/img/px_<name>.png."""
import pathlib

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAL = {"#": (23, 20, 12), "o": (255, 255, 255), "r": (214, 48, 39), "p": (247, 160, 190), "f": (232, 164, 0),
       "g": (150, 150, 150)}
out = ROOT / "manual" / "img"
out.mkdir(parents=True, exist_ok=True)
sprites, name, rows = {}, None, []
for path in sorted((ROOT / "assets" / "manual").glob("*.txt")):
    for line in path.read_text().splitlines() + [""]:
        if line.startswith("@"):
            name, rows = line[1:].strip(), []
        elif line.strip() and not line.startswith(";") and name:
            rows.append(line.strip())
        elif not line.strip() and name and rows:
            sprites[name] = rows
            name = None
s = 24
for n, rows in sprites.items():
    w = max(len(r) for r in rows)
    img = Image.new("RGBA", (w * s, len(rows) * s), (0, 0, 0, 0))
    px = img.load()
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            c = PAL.get(ch)
            if c:
                for dy in range(s - 3):
                    for dx in range(s - 3):
                        px[x * s + dx, y * s + dy] = (*c, 255)
    img.save(out / f"px_{n}.png")
# Preview sheet on both page colours.
names = list(sprites)
sheet = Image.new("RGB", (len(names) * 560, 1100), (255, 248, 220))
for i, n in enumerate(names):
    im = Image.open(out / f"px_{n}.png")
    sheet.paste(Image.new("RGB", (560, 550), (255, 210, 31)), (i * 560, 550))
    sheet.paste(im, (i * 560 + 20, 20), im)
    sheet.paste(im, (i * 560 + 20, 570), im)
sheet.thumbnail((2400, 1100))
sheet.save(ROOT / "manual" / "out" / "_culture.png")
print(", ".join(names))

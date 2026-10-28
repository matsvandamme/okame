#!/usr/bin/env python3
"""Tile PNGs into one labelled contact sheet: contact_sheet.py OUT.png IN1.png IN2.png ..."""
import os
import sys
from PIL import Image, ImageDraw

out, files = sys.argv[1], sorted(sys.argv[2:])
w, h, cols = 320, 160, 4
rows = (len(files) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (w + 10) + 10, rows * (h + 24) + 10), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).convert("RGB").resize((w, h), Image.NEAREST)
    x, y = 10 + (i % cols) * (w + 10), 10 + (i // cols) * (h + 24)
    sheet.paste(im, (x, y))
    d.text((x, y + h + 4), os.path.basename(f)[:-4], fill=(230, 230, 230))
sheet.save(out)

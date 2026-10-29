#!/usr/bin/env python3
"""Telegram profile picture for the Okame bot, drawn from the real adult sprite (assets/sprites/bird.txt).

    python tools/make_avatar.py [out_dir]   -> avatar_color.png, avatar_lcd.png (640x640)
"""
import pathlib
import sys

from PIL import Image, ImageDraw

root = pathlib.Path(__file__).resolve().parent.parent
out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else root / "out")
out.mkdir(parents=True, exist_ok=True)


def sprites():
    found, name = {}, None
    for line in (root / "assets/sprites/bird.txt").read_text().splitlines():
        if line.startswith("@ "):
            name, found[line[2:].strip()] = line[2:].strip(), []
        elif line.strip() and not line.startswith(";") and name:
            found[name].append(line.rstrip())
        else:
            name = None
    return found


def hat():
    rows, take = [], False
    for f in (root / "assets/sprites").glob("*.txt"):
        for line in f.read_text().splitlines():
            if line.startswith("@ hat_sakura"):
                take, rows = True, []
            elif take and line.strip():
                rows.append(line.rstrip())
            elif take:
                return rows
    return rows


bird = sprites()["adult_up"]
flower = hat()
W, H = len(bird[0]), len(bird)
HAT_AT = (6, 4)  # top-left of the sakura on the adult (lib/okame_core/src/ui.cpp drawHat OFF table)

INK, YELLOW, ORANGE, GREY, WHITE, PINK = "#1E2418", "#F6D24A", "#F08A3C", "#9AA0A6", "#F4F1E8", "#F7A8C4"
LCD_BG, LCD_INK = "#9EAA8A", "#1E2418"


CHEEK = {(12, 8), (13, 8), (12, 9), (13, 9)}  # the 2x2 ink block on the face is the cheek patch on the LCD


def colour(x, y, ch):
    """Paint the monochrome sprite as a cockatiel: yellow head and crest, orange cheek, grey body, white wing bar."""
    if ch == "#":
        return ORANGE if (x, y) in CHEEK else INK
    if ch == "w":
        return WHITE if (x + y) % 3 == 0 else GREY
    if ch == "o":
        if (y <= 9 and x >= 10) or (y == 10 and x >= 12):
            return YELLOW
        return GREY
    return None


def pixels(paint):
    px = {}
    for y, row in enumerate(bird):
        for x, ch in enumerate(row):
            c = paint(x, y, ch)
            if c:
                px[(x, y)] = c
    for y, row in enumerate(flower):
        for x, ch in enumerate(row):
            p = (HAT_AT[0] + x, HAT_AT[1] + y)
            if ch == "#":
                px[p] = "#C2185B" if paint is colour else LCD_INK
            elif ch == "o":
                px[p] = PINK if paint is colour else LCD_BG
    return px


def render(px, bg, frame, name, dot_gap):
    S, k = 640, 24 if frame else 25
    img = Image.new("RGB", (S, S), bg)
    d = ImageDraw.Draw(img)
    if frame:
        d.rounded_rectangle((70, 70, 570, 570), 90, fill="#1E2418")
        d.rounded_rectangle((100, 100, 540, 540), 60, fill=LCD_BG)
        for gy in range(100, 540, k // 2):  # faint unlit dots, like the real screen
            for gx in range(100, 540, k // 2):
                d.rectangle((gx + 1, gy + 1, gx + k // 2 - 2, gy + k // 2 - 2), fill="#929E7F")
    xs, ys = [x for x, _ in px], [y for _, y in px]
    if frame:
        ox, oy = (S - W * k) // 2 + k // 2, (S - H * k) // 2 + k
    else:  # centre the bird itself, a little low so the crest has room in Telegram's circle crop
        ox = (S - (max(xs) - min(xs) + 1) * k) // 2 - min(xs) * k
        oy = (S - (max(ys) - min(ys) + 1) * k) // 2 - min(ys) * k + k // 2
    for (x, y), c in px.items():
        g = dot_gap
        d.rectangle((ox + x * k + g, oy + y * k + g, ox + (x + 1) * k - 1 - g, oy + (y + 1) * k - 1 - g), fill=c)
    img.save(out / name)
    return img


render(pixels(colour), "#BFE3F2", False, "avatar_color.png", 0)
render(pixels(lambda x, y, ch: LCD_INK if ch == "#" or (ch == "w" and (x + y) % 2) else None), "#F6D24A", True,
       "avatar_lcd.png", 2)
print("wrote", out / "avatar_color.png", out / "avatar_lcd.png")

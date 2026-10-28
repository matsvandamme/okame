#!/usr/bin/env python3
"""Build the A5 booklet and impose it for printing on A4, double-sided, to fold and staple.

manual/booklet.html -> manual/out/okame-booklet-a5.pdf (reading order)
                    -> manual/out/okame-booklet-print-a4.pdf (2-up saddle-stitch order; print double-sided,
                       flip on the SHORT edge, then fold the stack in half and staple on the fold)
"""
import pathlib

from playwright.sync_api import sync_playwright
from pypdf import PageObject, PdfReader, PdfWriter, Transformation

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "manual" / "out"
OUT.mkdir(parents=True, exist_ok=True)
a5_pdf = OUT / "okame-booklet-a5.pdf"
with sync_playwright() as p:
    b = p.chromium.launch(channel="chrome")
    page = b.new_page(viewport={"width": 700, "height": 1000}, device_scale_factor=2)
    page.goto((ROOT / "manual" / "booklet.html").as_uri(), wait_until="networkidle")
    page.evaluate("document.fonts.ready")
    page.wait_for_timeout(800)
    page.pdf(path=str(a5_pdf), format="A5", print_background=True, prefer_css_page_size=True)
    for i, el in enumerate(page.query_selector_all("section.page"), 1):
        el.screenshot(path=str(OUT / f"a5_page_{i:02d}.png"))
    b.close()

pages = PdfReader(a5_pdf).pages
n = len(pages)
assert n % 4 == 0, f"{n} pages: a folded booklet needs a multiple of 4"
w, h = float(pages[0].mediabox.width), float(pages[0].mediabox.height)
order = []
for i in range(n // 4):  # each sheet: front (outer pair), back (inner pair)
    order.append((n - 2 * i, 1 + 2 * i))
    order.append((2 + 2 * i, n - 1 - 2 * i))
writer = PdfWriter()
for left, right in order:
    side = PageObject.create_blank_page(width=2 * w, height=h)
    side.merge_transformed_page(pages[left - 1], Transformation().translate(0, 0))
    side.merge_transformed_page(pages[right - 1], Transformation().translate(w, 0))
    writer.add_page(side)
with open(OUT / "okame-booklet-print-a4.pdf", "wb") as f:
    writer.write(f)
print("sides:", " ".join(f"[{l}|{r}]" for l, r in order))

#!/usr/bin/env python3
"""Print manual/manual.html to manual/out/okame-manual.pdf (A4) and PNG previews of each page."""
import pathlib

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
out = ROOT / "manual" / "out"
out.mkdir(parents=True, exist_ok=True)
url = (ROOT / "manual" / "manual.html").as_uri()
with sync_playwright() as p:
    b = p.chromium.launch(channel="chrome")
    page = b.new_page(viewport={"width": 900, "height": 1200}, device_scale_factor=1.5)
    page.goto(url, wait_until="networkidle")
    page.evaluate("document.fonts.ready")
    page.wait_for_timeout(800)
    page.pdf(path=str(out / "okame-manual.pdf"), format="A4", print_background=True, prefer_css_page_size=True)
    for i, el in enumerate(page.query_selector_all("section.page"), 1):
        el.screenshot(path=str(out / f"page_{i}.png"))
    b.close()
print(out / "okame-manual.pdf")

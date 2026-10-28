#!/usr/bin/env python3
"""Screenshot the phone page (mock server or real dongle) at iPhone size, one shot per tab.

usage: shoot_web.py BASE_URL OUT_DIR [prefix]
"""
import pathlib
import sys

from playwright.sync_api import sync_playwright

base, out = sys.argv[1], pathlib.Path(sys.argv[2])
prefix = sys.argv[3] if len(sys.argv) > 3 else "web"
out.mkdir(parents=True, exist_ok=True)
with sync_playwright() as p:
    b = p.chromium.launch(channel="chrome")
    p.selectors.set_test_id_attribute("id")
    ctx = b.new_context(viewport={"width": 390, "height": 844}, device_scale_factor=2, is_mobile=True, has_touch=True)
    page = ctx.new_page()
    errors = []
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.on("console", lambda m: m.type == "error" and errors.append(m.text))
    page.goto(base, wait_until="domcontentloaded")
    page.wait_for_timeout(1500)
    page.screenshot(path=str(out / f"{prefix}_1_care.png"), full_page=True)
    for i, tab in enumerate(["tune", "photo", "settings"], 2):
        page.click(f'[data-tab="{tab}"]')
        page.wait_for_timeout(700)
        if tab == "settings" and page.is_visible("#locked"):
            page.screenshot(path=str(out / f"{prefix}_{i}_settings_locked.png"), full_page=True)
            page.fill("#codeInput", "4721")  # the page submits by itself on the 4th digit
            page.wait_for_timeout(1500)
        page.screenshot(path=str(out / f"{prefix}_{i}_{tab}.png"), full_page=True)
    print("errors:", errors or "none")
    b.close()

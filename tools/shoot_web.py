#!/usr/bin/env python3
"""Screenshot the phone page at iPhone size (WebKit, like Safari), one scenario per mock-server flag set.

usage: shoot_web.py OUT_DIR [--url http://127.0.0.1:8765] [--scenarios full,fuss,away,family,gone,setup]

Without --url it starts tools/mock_server.py itself on a free port per scenario. Each scenario produces one
full-page shot, one per tab, and one per card, plus a contact sheet. Needs Playwright with WebKit:
    uv run --with playwright python -m playwright install webkit
    uv run --with playwright --with pillow python tools/shoot_web.py out/web/shots
"""
import pathlib
import socket
import subprocess
import sys
import time
import urllib.request

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCENARIOS = {
    "full": ["--named", "--full"],
    "fuss": ["--named", "--full", "--fuss"],
    "away": ["--named", "--full", "--away", "--family"],
    "golden": ["--named", "--full", "--golden"],
    "gone": ["--named", "--full", "--gone"],
    "setup": ["--setup"],
    "egg": [],
}
TABS = ["care", "play", "world", "tune", "photo", "settings"]

args = sys.argv[1:]
out = pathlib.Path(args[0] if args and not args[0].startswith("--") else "out/web/shots")
url = next((a.split("=", 1)[1] for a in args if a.startswith("--url=")), None)
wanted = next((a.split("=", 1)[1].split(",") for a in args if a.startswith("--scenarios=")), list(SCENARIOS))
out.mkdir(parents=True, exist_ok=True)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def mock(flags):
    port = free_port()
    proc = subprocess.Popen([sys.executable, str(ROOT / "tools" / "mock_server.py"), str(port), *flags],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(50):
        try:
            urllib.request.urlopen(f"http://127.0.0.1:{port}/api/state", timeout=1)
            return proc, f"http://127.0.0.1:{port}"
        except Exception:  # noqa: BLE001
            time.sleep(0.1)
    proc.kill()
    sys.exit("mock server did not start")


def shoot(pg, base, prefix):
    errors = []
    pg.on("pageerror", lambda e: errors.append(str(e)))
    pg.on("console", lambda m: m.type == "error" and "status of 4" not in m.text and errors.append(m.text))
    pg.goto(base + "/", wait_until="commit")
    pg.wait_for_timeout(1500)
    pg.screenshot(path=str(out / f"{prefix}_page.png"), full_page=True)
    for tab in TABS:
        btn = pg.locator(f'[data-tab="{tab}"]')
        if not btn.count():
            continue
        btn.click()
        pg.wait_for_timeout(400)
        pg.screenshot(path=str(out / f"{prefix}_tab_{tab}.png"), full_page=True)
        for card in pg.locator("section.card:visible").all():
            cid = card.get_attribute("id")
            if cid:
                card.screenshot(path=str(out / f"{prefix}_{cid}.png"))
    return errors


with sync_playwright() as p:
    browser = p.webkit.launch()
    ctx = browser.new_context(**p.devices["iPhone 13"])
    failures = 0
    for name in wanted:
        proc, base = (None, url) if url else mock(SCENARIOS[name])
        pg = ctx.new_page()
        errs = shoot(pg, base, name)
        pg.close()
        if proc:
            proc.kill()
        print(f"{name}: {'ok' if not errs else 'ERRORS ' + '; '.join(errs)}")
        failures += bool(errs)
    browser.close()
subprocess.call([sys.executable, str(ROOT / "tools" / "contact_sheet.py"), str(out / "sheet_cards.png"),
                 *[str(f) for f in sorted(out.glob("*_*Card.png"))]])
sys.exit(1 if failures else 0)

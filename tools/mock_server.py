#!/usr/bin/env python3
"""Serve the built phone page with a fake API, for screenshots without the dongle.

usage: mock_server.py [port] [--setup] [--named]
"""
import json
import pathlib
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGE = (ROOT / "out" / "web" / "index.html").read_bytes()
FRAMES = (ROOT / "out" / "web" / "frames.txt").read_text().split()
SETUP = "--setup" in sys.argv
STATE = {
    "name": "Mochi" if "--named" in sys.argv else "", "stage": "adult", "variant": "pearl", "hunger": 3,
    "happy": 2, "poop": 1, "sick": False, "lightsOff": False, "weight": 94, "ageDays": 5, "mood": "content",
    "attention": False, "tune": "02431", "tuneLevel": 2, "night": False, "setup": SETUP,
    "host": "mochi" if "--named" in sys.argv else "okame", "fw": "0.1.0",
    "game": {"type": "duet", "phase": "listening", "round": 1, "rounds": 3, "wins": 1} if "--game" in sys.argv else None,
}
NETS = ["Home Wi-Fi", "Phone hotspot"]
TOKEN = "f" * 32


def frame():
    return FRAMES[int(time.time() / 0.15) % len(FRAMES)]


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def authed(self):
        return SETUP or self.headers.get("X-Okame-Token") == TOKEN

    def do_GET(self):
        if self.path == "/":
            return self.send(200, PAGE, "text/html")
        if self.path == "/api/state":
            return self.send(200, {**STATE, "frame": frame()})
        if self.path == "/api/settings":
            if not self.authed():
                return self.send(401, {"error": "pair first"})
            return self.send(200, {"networks": NETS, "fw": "0.1.0", "ota": "up to date", "ap": "OKAME-F3A"})
        if self.path == "/api/scan":
            return self.send(200, [{"ssid": "Home Wi-Fi", "rssi": -48}, {"ssid": "Phone hotspot", "rssi": -61},
                                   {"ssid": "Neighbour 5G", "rssi": -80}])
        if self.path == "/api/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.end_headers()
            try:
                while True:
                    self.wfile.write(f"event: frame\ndata: {frame()}\n\n".encode())
                    self.wfile.flush()
                    time.sleep(0.15)
            except (BrokenPipeError, ConnectionResetError):
                return
        self.send(404, {"error": "not found"})

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        q = {k: v[0] for k, v in parse_qs(self.rfile.read(n).decode()).items()}
        if self.path == "/api/act":
            return self.send(200, {"result": "ok"})
        if self.path == "/api/name":
            STATE["name"] = q.get("name", "")
            STATE["host"] = STATE["name"].lower()
            return self.send(200, {"host": STATE["host"]})
        if self.path == "/api/pair":
            if q.get("code") == "4721":
                return self.send(200, {"token": TOKEN})
            return self.send(403, {"error": "wrong code", "triesLeft": 4})
        if self.path in ("/api/tune", "/api/networks/add", "/api/networks/remove", "/api/new-egg", "/api/ota/check"):
            return self.send(200, {"ok": True})
        if self.path == "/api/game":
            STATE["game"] = {"type": q.get("g"), "phase": "listening" if q.get("g") == "duet" else "hidden",
                             "round": 0, "rounds": 3 if q.get("g") == "duet" else 5, "wins": 0, "hits": 0}
            return self.send(200, {"ok": True})
        if self.path == "/api/game/input":
            return self.send(200, {"game": STATE.get("game")})
        if self.path == "/api/pair/request":
            return self.send(200, {"ok": True})
        if self.path == "/api/practice":
            return self.send(200, {"learned": True, "level": 3})
        self.send(404, {"error": "not found"})


port = int(next((a for a in sys.argv[1:] if a.isdigit()), "8765"))
srv = ThreadingHTTPServer(("127.0.0.1", port), H)
print(f"mock on http://127.0.0.1:{port}")
srv.serve_forever()

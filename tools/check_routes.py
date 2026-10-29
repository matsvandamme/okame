#!/usr/bin/env python3
"""Check that every api() call in web/index.html uses the HTTP method the firmware route accepts.

api(path) and api(path, null) are GET; api(path, {...}) is POST. A mismatch makes the dongle answer 404.
"""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
cpp = (root / "src/web.cpp").read_text()
page = (root / "web/index.html").read_text()
routes = dict(re.findall(r'server\.on\("(/api/[^"]+)",\s*HTTP_(\w+)', cpp))
bad = 0
for path, body in re.findall(r'api\(\s*["`](/api/[^"`$]+)["`]\s*(?:,\s*([^,)]+))?', page):
    method = "POST" if body and body.strip() not in ("null", "undefined") else "GET"
    want = routes.get(path)
    if want not in (method, "ANY"):
        bad += 1
        print(f"MISMATCH {path}: page sends {method}, firmware {'has no route' if not want else 'accepts ' + want}")
print(f"{'FAIL' if bad else 'OK'}: checked page api() calls against {len(routes)} firmware routes")
sys.exit(1 if bad else 0)

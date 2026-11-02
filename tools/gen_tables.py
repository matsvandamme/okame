#!/usr/bin/env python3
"""One source for the shared tables: assets/tables.json.

Writes lib/okame_core/src/tables.h (names and prices the firmware uses, with static_asserts against the enums),
rewrites the generated block in relay/okame_relay.py, and gives tools/embed_web.py the JSON for the page's `T`.

    python tools/gen_tables.py          regenerate
    python tools/gen_tables.py --check  fail if anything is out of date (CI)
"""
import json
import pathlib
import pprint
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "tables.json"
HEADER = ROOT / "lib" / "okame_core" / "src" / "tables.h"
RELAY = ROOT / "relay" / "okame_relay.py"
MARK_START, MARK_END = "# --- generated from assets/tables.json by tools/gen_tables.py: do not edit ---", "# --- end generated ---"


def load():
    t = json.loads(SRC.read_text())
    t.pop("_comment", None)
    return t


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def header(t):
    def arr(name, items, ctype="const char* const"):
        return f"constexpr {ctype} {name}[] = {{{', '.join(items)}}};\n"

    out = ["// Generated from assets/tables.json by tools/gen_tables.py. Do not edit: change the JSON and rerun.\n",
           "#pragma once\n#include <stdint.h>\n\nnamespace tables {\n"]
    out.append(arr("SPECIES", [cstr(s["key"]) for s in t["species"]]))
    out.append(arr("STAGES", [cstr(s) for s in t["stages"]]))
    out.append(arr("VARIANTS", [cstr(s) for s in t["variants"]]))
    out.append(arr("MOODS", [cstr(s) for s in t["moods"]]))
    out.append(arr("RESULTS", [cstr(s) for s in t["results"]]))
    out.append(arr("OCCASIONS", [cstr(s) for s in t["occasions"]]))
    out.append(arr("FRIENDS", [cstr(s["key"]) for s in t["friends"]]))
    out.append(arr("PLACES", [cstr(s["key"]) for s in t["places"]]))
    out.append(arr("PLACE_LCD", [cstr(s["lcd"]) for s in t["places"]]))
    out.append(arr("PLACE_MIN_DAYS", [str(s["minDays"]) for s in t["places"]], "uint8_t"))
    out.append(arr("TOYS", [cstr(s["key"]) for s in t["toys"]]))
    out.append(arr("TOY_PRICE", [str(s["price"]) for s in t["toys"]], "uint16_t"))
    out.append(arr("HATS", [cstr(s["key"]) for s in t["hats"]]))
    out.append(arr("PARTY_DAYS", [str(d) for d in t["partyDays"]], "uint16_t"))
    out.append(f"constexpr uint16_t HAT_PRICE = {t['prices']['hat']}, TREAT_PRICE = {t['prices']['treat']};\n")
    out.append(f"constexpr uint8_t HAT_GIFTABLE = {t['giftable']};  // accessories below this index can be gifted or bought\n")
    out.append("}  // namespace tables\n")
    return "".join(out)


def relay_block(t):
    keep = {k: t[k] for k in ("species", "friends", "places", "toys", "hats", "prices", "omikuji", "partyDays")}
    return f"{MARK_START}\nTABLES = {pprint.pformat(keep, width=110, sort_dicts=False)}\n{MARK_END}\n"


def relay_text(t):
    src = RELAY.read_text()
    pattern = re.compile(re.escape(MARK_START) + r".*?" + re.escape(MARK_END) + r"\n", re.S)
    if not pattern.search(src):
        return src  # the relay doesn't carry the block yet
    return pattern.sub(lambda m: relay_block(t), src)


def main():
    t = load()
    want = {HEADER: header(t), RELAY: relay_text(t)}
    if "--check" in sys.argv:
        stale = [str(p.relative_to(ROOT)) for p, text in want.items() if not p.exists() or p.read_text() != text]
        if stale:
            sys.exit("out of date (run tools/gen_tables.py): " + ", ".join(stale))
        print("tables up to date")
        return
    for p, text in want.items():
        if not p.exists() or p.read_text() != text:
            p.write_text(text)
            print(f"wrote {p.relative_to(ROOT)}")


if __name__ == "__main__":
    main()

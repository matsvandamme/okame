#!/usr/bin/env python3
"""Relay tests with Telegram, the AI and the egg stubbed out. Uses a throwaway OKAME_HOME.

    uv run --with aiohttp --with anthropic --with pillow python relay/test_relay.py
"""
import asyncio
import json
import os
import pathlib
import sys
import tempfile
import time

home = pathlib.Path(tempfile.mkdtemp(prefix="okame-test-"))
(home / "config.json").write_text(json.dumps({
    "device_key": "a" * 64, "partner_chats": [100], "tz": "Europe/Brussels",
    "weather": {"lat": 51.0, "lon": 4.0, "place": "Home"},  # made-up test values: real ones live only on the relay
    "dates": [{"m": 6, "d": 1, "kind": 1, "who": "Hana", "year": 2000}]}))
(home / "telegram_token").write_text("fake")
os.environ["OKAME_HOME"] = str(home)
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import okame_relay as r  # noqa: E402

MATTHIEU, HARUKI = 100, 200
sent = []


async def fake_say(chat, text, **k):
    sent.append((chat, text))


async def fake_show(chat, cap, **k):
    sent.append((chat, cap))


async def fake_tg(method, **k):
    sent.append((k.get("chat_id"), f"<{method}>"))
    return {"ok": True, "result": {"message_id": 1}}


r.say, r.show, r.tg = fake_say, fake_show, fake_tg
for chat, name in ((MATTHIEU, "Matthieu"), (HARUKI, "Haruki")):
    r.db.execute("insert into users values(?,?,?,?)", (chat, name, "en", time.time()))


def msg(chat, text):
    return {"chat": {"id": chat}, "from": {"first_name": "Matthieu" if chat == MATTHIEU else "Haruki"}, "text": text}


def queued(kind):
    return [json.loads(x["payload"]) for x in r.db.execute("select * from queue where kind=? order by id", (kind,))]


def said(chat):
    return [t for c, t in sent if c == chat]


fails = 0


def check(name, ok):
    global fails
    fails += not ok
    print(("PASS " if ok else "FAIL ") + name)


async def main():
    # Old egg: new commands are refused politely instead of being lost as "unknown".
    r.device["state"] = {"fw": "0.3.2"}
    await r.handle(msg(MATTHIEU, "/hug"))
    check("old egg: /hug asks for an update", "update" in said(MATTHIEU)[-1] and not queued("hug"))

    r.device["state"] = {"fw": "0.4.0-dev"}
    await r.handle(msg(MATTHIEU, "/hug"))
    check("/hug queues a hug from MATTHIEU (screen-safe)", queued("hug") == [{"from": "MATTHIEU"}])

    sent.clear()
    await r.on_event({"type": "love"})
    check("💛 reaches Matthieu only", said(MATTHIEU) and not said(HARUKI))
    await r.on_event({"type": "love"})
    check("💛 is rate-limited", len(said(MATTHIEU)) == 1)

    for t in TESTS:
        await t()
    print("\n" + ("ALL PASSED" if not fails else f"{fails} FAILED"))
    sys.exit(1 if fails else 0)


TESTS = []  # later features append their test coroutines here


async def t_home():
    from datetime import date
    today = date(2026, 11, 20)
    check("parse 15/12 -> this year", r.parse_day("15/12", today) == date(2026, 12, 15))
    check("parse 10/1 -> next year", r.parse_day("10/1", today) == date(2027, 1, 10))
    check("parse ISO", r.parse_day("2026-12-15", today) == date(2026, 12, 15))
    check("parse junk", r.parse_day("next tuesday", today) is None)
    await r.handle(msg(MATTHIEU, "/home 2030-01-02"))
    check("/home queues the date", queued("home")[-1] == {"y": 2030, "m": 1, "d": 2})
    await r.handle(msg(MATTHIEU, "/home off"))
    check("/home off clears it", queued("home")[-1] == {"y": 0, "m": 0, "d": 0} and r.kv_get("home") is None)
TESTS.append(t_home)


async def t_words():
    async def fake_ask(system, prompt, **k):
        return {"おやすみ": "oyasumi"}.get(prompt, "")
    r.ask = fake_ask
    await r.handle(msg(MATTHIEU, "/teach おやすみ"))
    check("/teach Japanese -> romaji", queued("word")[-1] == {"text": "OYASUMI"})
    await r.handle(msg(MATTHIEU, "/teach I love you"))
    check("/teach English stays", queued("word")[-1] == {"text": "I LOVE YOU"})
    n = len(queued("word"))
    await r.handle(msg(MATTHIEU, "/teach oyasumi"))
    check("/teach a known word is not resent", len(queued("word")) == n)
    await r.handle(msg(MATTHIEU, "/forget oyasumi"))
    check("/forget", queued("forget")[-1] == {"text": "OYASUMI"} and r.kv_get("words") == ["I LOVE YOU"])
TESTS.append(t_words)


async def t_weather_and_dates():
    from datetime import datetime
    c = r.classify_weather
    check("weather: drizzle is rain", c(53, 12) == "rain")
    check("weather: showers are rain", c(81, 15) == "rain")
    check("weather: snow", c(73, -1) == "snow")
    check("weather: hot", c(1, 31) == "hot")
    check("weather: cold", c(2, 1) == "cold")
    check("weather: mild clear", c(0, 14) == "clear")
    await r.send_dates()
    check("dates go to the egg", queued("dates")[-1] == {"list": [{"m": 6, "d": 1, "kind": 1}], "who": "HANA"})
    sent.clear()
    await r.birthdays(datetime(2027, 5, 31, 8, 0, tzinfo=r.TZ))
    check("day before: heads-up to Matthieu only, with her age",
          any("tomorrow" in t and "(27)" in t for t in said(MATTHIEU)) and not said(HARUKI))
    sent.clear()
    await r.birthdays(datetime(2027, 6, 1, 8, 0, tzinfo=r.TZ))
    check("on the day: wishes for everyone", said(MATTHIEU) and any("Happy birthday" in t for t in said(HARUKI)))
TESTS.append(t_weather_and_dates)


async def t_catch():
    import random
    random.seed(1)
    r.CATCH_WINDOW = 0.2
    r.device["state"] = {"fw": "0.4.0", "bestRain": 5}
    await r.handle(msg(MATTHIEU, "/catch"))
    g = r.catches[MATTHIEU]

    def tap(col, rnd=None):
        return {"id": "q", "data": f"catch:{g['id']}:{g['round'] if rnd is None else rnd}:{col}",
                "message": {"chat": {"id": MATTHIEU}}}
    gold = g["golden"]
    await r.on_callback(tap(g["col"]))
    check("a hit scores", g["score"] == (3 if gold else 1) and g["lives"] == 3)
    await r.on_callback(tap((g["col"] + 1) % 5))
    check("a wrong tap costs a life", g["lives"] == 2)
    before = g["round"]
    await r.on_callback(tap(0, rnd=before - 1))
    check("a stale tap is ignored", g["round"] == before)
    await asyncio.sleep(0.7)
    check("a timeout costs a life", g["lives"] == 1)
    while MATTHIEU in r.catches:
        await r.on_callback(tap(g["col"]))
    check("game ends; a new best goes to the egg as rival", queued("rival") and r.kv_get("catch_best") == g["score"])
    check("Haruki hears about it", any("Feather Catch" in t for t in said(HARUKI)))
    sent.clear()
    await r.on_event({"type": "rain", "score": 99})
    check("her record reaches Matthieu, beating him", any("beats your" in t for t in said(MATTHIEU)))
TESTS.append(t_catch)


async def t_whats_new():
    sent.clear()
    await r.whats_new()
    await r.whats_new()
    check("what's new: once each, the right text", len(said(MATTHIEU)) == 1 and "/catch" in said(MATTHIEU)[0]
          and len(said(HARUKI)) == 1 and "Janken" in said(HARUKI)[0])
TESTS.append(t_whats_new)

if __name__ == "__main__":
    asyncio.run(main())

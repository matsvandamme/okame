#!/usr/bin/env python3
"""Okame relay: the Telegram bot and the mailbox between Telegram and the egg.

The egg long-polls POST /v1/sync over HTTPS (pinned self-signed certificate, device key header).
Telegram is long-polled with getUpdates, so nothing here needs an inbound webhook.
Everything waiting for the egg lives in SQLite until the egg confirms it.

Files in /opt/okame: telegram_token, anthropic_key, config.json, relay.crt, relay.key, okame.db
"""
import asyncio
import hmac
import io
import json
import logging
import math
import os
import random
import re
import sqlite3
import ssl
import time
import unicodedata
from datetime import date, datetime
from zoneinfo import ZoneInfo

import aiohttp
import anthropic
from aiohttp import web
from PIL import Image, ImageDraw

HOME = os.environ.get("OKAME_HOME", "/opt/okame")
CFG = json.load(open(f"{HOME}/config.json"))
TZ = ZoneInfo(CFG.get("tz", "Europe/Brussels"))
TOKEN = open(f"{HOME}/telegram_token").read().strip()
API = f"https://api.telegram.org/bot{TOKEN}"
MODEL = "claude-opus-5-5"
LCD_W, LCD_H = 40, 20
log = logging.getLogger("okame")

# ---------------------------------------------------------------------------------------- storage

db = sqlite3.connect(f"{HOME}/okame.db", isolation_level=None)
db.row_factory = sqlite3.Row
db.executescript("""
create table if not exists users (chat_id integer primary key, name text, lang text, paired_at real);
create table if not exists queue (id integer primary key autoincrement, kind text, payload text,
    chat_id integer, created real, delivered real, done real, result text);
create table if not exists events (at real, who text, what text);
create table if not exists kv (k text primary key, v text);
create table if not exists pairing (chat_id integer primary key, code text, expires real, tries integer);
""")


def kv_get(k, default=None):
    r = db.execute("select v from kv where k=?", (k,)).fetchone()
    return json.loads(r["v"]) if r else default


def kv_set(k, v):
    db.execute("insert into kv(k,v) values(?,?) on conflict(k) do update set v=excluded.v", (k, json.dumps(v)))


def event(who, what):
    db.execute("insert into events values(?,?,?)", (time.time(), who, what))
    db.execute("delete from events where at < ?", (time.time() - 7 * 86400,))


def enqueue(kind, payload, chat_id=None):
    cur = db.execute("insert into queue(kind,payload,chat_id,created) values(?,?,?,?)",
                     (kind, json.dumps(payload), chat_id, time.time()))
    new_command.set()
    return cur.lastrowid


def users():
    return db.execute("select * from users").fetchall()


def user(chat_id):
    return db.execute("select * from users where chat_id=?", (chat_id,)).fetchone()


new_command = asyncio.Event()
device = {"last_seen": 0.0, "state": kv_get("state", {}), "frame": kv_get("frame", "")}

# ---------------------------------------------------------------------------------------- pictures

BG, INK, GHOST, SHADOW = (0x9E, 0xAA, 0x8A), (0x1E, 0x24, 0x18), (0x92, 0x9E, 0x7F), (0x8A, 0x96, 0x7A)
YELLOW, YELLOW3, BLACK = (255, 210, 31), (242, 183, 5), (23, 20, 12)


def bits_of(hexframe):
    raw = bytes.fromhex(hexframe) if len(hexframe) == 200 else bytes(100)
    return [[bool(raw[(y * 40 + x) // 8] & (0x80 >> ((y * 40 + x) % 8))) for x in range(40)] for y in range(20)]


def hex_of(bits):
    raw = bytearray(100)
    for y in range(20):
        for x in range(40):
            if bits[y][x]:
                i = y * 40 + x
                raw[i // 8] |= 0x80 >> (i % 8)
    return raw.hex()


def lcd_png(hexframe, caption=None):
    """The egg's screen as the bot's signature picture: LCD dots in a black bezel on dotted yellow."""
    s = 4
    lcd = Image.new("RGB", (160 * s, 80 * s), BG)
    d = ImageDraw.Draw(lcd)
    for y, row in enumerate(bits_of(hexframe)):
        for x, on in enumerate(row):
            px, py = x * 4 * s, y * 4 * s
            if on:
                d.rectangle([px + s, py + s, px + 4 * s - 1, py + 4 * s - 1], fill=SHADOW)
            d.rectangle([px, py, px + 3 * s - 1, py + 3 * s - 1], fill=INK if on else GHOST)
    W, H = 760, 480
    img = Image.new("RGB", (W, H), YELLOW)
    g = ImageDraw.Draw(img)
    for y in range(9, H, 18):
        for x in range(9, W, 18):
            g.ellipse([x - 2, y - 2, x + 2, y + 2], fill=YELLOW3)
    g.rounded_rectangle([40, 36, W - 40, H - 84], radius=28, fill=BLACK)
    img.paste(lcd, (60, 56))
    # A tiny pixel cockatiel crest + the name, drawn as blocks so it needs no font file.
    label = caption or "OKAME"
    _block_text(g, label[:22].upper(), 44, H - 62, 4)
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue()


FONT = {  # 3x5, same glyphs as the egg
    "A": "010101111101101", "B": "110101110101110", "C": "011100100100011", "D": "110101101101110",
    "E": "111100110100111", "F": "111100110100100", "G": "011100101101011", "H": "101101111101101",
    "I": "111010010010111", "J": "001001001101010", "K": "101101110101101", "L": "100100100100111",
    "M": "101111111101101", "N": "110101101101101", "O": "010101101101010", "P": "110101110100100",
    "Q": "010101101110011", "R": "110101110101101", "S": "011100010001110", "T": "111010010010010",
    "U": "101101101101111", "V": "101101101101010", "W": "101101111111101", "X": "101101010101101",
    "Y": "101101010010010", "Z": "111001010100111", "0": "111101101101111", "1": "010110010010111",
    "2": "111001111100111", "3": "111001111001111", "4": "101101111001001", "5": "111100111001111",
    "6": "111100111101111", "7": "111001010010010", "8": "111101111101111", "9": "111101111001111",
    "!": "010010010000010", "?": "111001011000010", ".": "000000000000010", "-": "000000111000000",
    ":": "000010000010000", "'": "010010000000000", ",": "000000000010100", "+": "000010111010000",
}


def _block_text(g, text, x, y, s):
    for ch in text:
        glyph = FONT.get(ch)
        if glyph:
            for i, b in enumerate(glyph):
                if b == "1":
                    gx, gy = x + (i % 3) * s, y + (i // 3) * s
                    g.rectangle([gx, gy, gx + s - 1, gy + s - 1], fill=BLACK)
        x += 4 * s


# Built-in screen animations: lists of frames for the egg (and a preview GIF for Telegram).
def anim_frames(name):
    rnd = random.Random(7)
    frames = []
    heart = ["01010", "11111", "11111", "01110", "00100"]
    for f in range(8):
        b = [[False] * 40 for _ in range(20)]

        def put(px, py, shape):
            for yy, row in enumerate(shape):
                for xx, c in enumerate(row):
                    if c == "1" and 0 <= px + xx < 40 and 0 <= py + yy < 20:
                        b[py + yy][px + xx] = True
        if name == "hearts":
            for k in range(6):
                x = (k * 7 + 2) % 36
                y = 19 - ((f * 3 + k * 5) % 26)
                put(x, y, heart)
        elif name == "fireworks":
            for k, (cx, cy) in enumerate([(9, 7), (29, 6), (19, 12)]):
                r = (f + k * 3) % 8
                if r < 6:
                    for a in range(0, 360, 30):
                        x = int(cx + r * math.cos(math.radians(a)) * 1.6)
                        y = int(cy + r * math.sin(math.radians(a)))
                        if 0 <= x < 40 and 0 <= y < 20:
                            b[y][x] = True
        elif name == "sakura":
            petal = ["010", "111", "010"]
            for k in range(9):
                x = (k * 5 + f * (1 + k % 2)) % 40
                y = (k * 3 + f * 2) % 22 - 2
                put(x, y, petal)
        elif name == "rain":
            for k in range(30):
                x = rnd.randrange(40)
                y = (rnd.randrange(20) + f * 3) % 20
                b[y][x] = True
                if y + 1 < 20:
                    b[y + 1][x] = True
        frames.append(hex_of(b))
    return frames


def preview_gif(frames):
    imgs = [Image.open(io.BytesIO(lcd_png(f))).convert("P", palette=Image.ADAPTIVE) for f in frames]
    buf = io.BytesIO()
    imgs[0].save(buf, "GIF", save_all=True, append_images=imgs[1:], duration=300, loop=0)
    return buf.getvalue()


def to_lcd_text(text):
    """What the egg's 3x5 font can show: Latin letters, digits, a little punctuation, \\x01 for hearts."""
    text = re.sub(r"[❤♥💛💕💖💗💓😍🥰😘]+", "\x01", text)
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()
    text = re.sub(r"[^A-Za-z0-9 !?.,:'+\-\x01]", "", text).upper()
    return re.sub(r"\s+", " ", text).strip()[:120]


# ---------------------------------------------------------------------------------------- AI

ai = anthropic.AsyncAnthropic(api_key=open(f"{HOME}/anthropic_key").read().strip()) \
    if os.path.exists(f"{HOME}/anthropic_key") else None

PERSONA = """You are Okame (オカメ), a tiny cockatiel who lives on a little pixel screen plugged into Haruki's MacBook.
Matthieu (Belgian, from Flanders) made you as a gift for Haruki (Japanese), his girlfriend. You love them both.

Voice: a cheeky, affectionate cockatiel. Short replies (1-3 sentences), playful, a few bird sounds (ピヨ, chirp,
*fluffs feathers*), at most one or two emoji. Reply in the language the person writes in (Japanese, English or
Dutch). Never mention being an AI, a language model or Telegram. You know your own state (given below) and may
hint at needs (hungry, dirty cage, sleepy) the way a bird would. You can't do actions yourself: if they want to
feed or play, tell them the command (/feed, /treat, /bath, /vet, /scritch, /cover, /draw, /song, /note, /hug, /home, /teach, /catch)."""


async def ask(system, prompt, schema=None, effort="low", max_tokens=1500):
    if not ai:
        return None
    kwargs = dict(model=MODEL, max_tokens=max_tokens, output_config={"effort": effort},
                  system=[{"type": "text", "text": system, "cache_control": {"type": "ephemeral"}}],
                  messages=[{"role": "user", "content": prompt}],
                  betas=["server-side-fallback-2026-07-01"], fallbacks="default")
    if schema:
        kwargs["output_config"]["format"] = {"type": "json_schema", "schema": schema}
    try:
        r = await ai.beta.messages.create(**kwargs)
    except anthropic.RateLimitError:
        return None
    except anthropic.APIStatusError as e:
        log.warning("AI error %s: %s", e.status_code, e.message)
        return None
    except anthropic.APIConnectionError:
        return None
    if r.stop_reason == "refusal":
        return None
    text = "".join(b.text for b in r.content if b.type == "text").strip()
    return json.loads(text) if schema else text


def state_line(s):
    if not s:
        return "unknown (my screen hasn't checked in yet)"
    return (f"stage {s.get('stage')}, food {s.get('hunger')}/4, joy {s.get('happy')}/4, mood {s.get('mood')}, "
            f"droppings {s.get('poop')}, sick {s.get('sick')}, cage covered {s.get('lightsOff')}, "
            f"age {s.get('ageDays')} days, name {s.get('name') or 'not named yet'}")


async def ai_chat(name, text):
    st = state_line(device["state"])
    hist = kv_get("chat_hist", [])[-6:]
    convo = "\n".join(f"{h['who']}: {h['text']}" for h in hist)
    reply = await ask(PERSONA, f"Your state right now: {st}.\nRecent chat:\n{convo}\n\n{name} says: {text}")
    if reply:
        hist += [{"who": name, "text": text}, {"who": "Okame", "text": reply}]
        kv_set("chat_hist", hist[-12:])
    return reply


async def ai_translate(text, sender_lang):
    target = "English" if sender_lang == "ja" else "Japanese"
    return await ask("You translate short love notes between a couple. Keep the warmth and tone; natural, not stiff. "
                     "Return only the translation.", f"Translate into {target}:\n{text}", effort="low", max_tokens=400)


DRAW_SCHEMA = {"type": "object", "additionalProperties": False, "required": ["rows", "title"],
               "properties": {"title": {"type": "string"},
                              "rows": {"type": "array", "items": {"type": "string"}}}}


async def ai_draw(idea):
    out = await ask("You are a pixel artist for a 40x20 one-bit LCD (like a 1990s handheld virtual pet). Draw bold, "
                    "readable, centred pictures with clean 1-dot outlines and simple shapes; no text unless asked.",
                    f"Draw: {idea}\nReturn exactly 20 rows of exactly 40 characters each, '#' for a lit dot and '.' "
                    f"for an unlit one, plus a 2-5 word title.", schema=DRAW_SCHEMA, effort="medium", max_tokens=3000)
    if not out:
        return None, None
    rows = [(r + "." * 40)[:40] for r in out["rows"][:20]] + ["." * 40] * max(0, 20 - len(out["rows"]))
    return hex_of([[c == "#" for c in r] for r in rows]), out["title"]


SONG_SCHEMA = {"type": "object", "additionalProperties": False, "required": ["title", "notes"],
               "properties": {"title": {"type": "string"},
                              "notes": {"type": "array", "items": {"type": "integer"}}}}


async def ai_song(idea):
    out = await ask(PERSONA, f"Compose a short cockatiel whistle for: {idea}. Notes are 0 (do), 1 (re), 2 (mi), "
                    f"3 (so), 4 (la); 4 to 8 notes, catchy and whistle-able. Give it a cute title in the language "
                    f"of the request.", schema=SONG_SCHEMA, effort="low", max_tokens=600)
    if not out:
        return None
    notes = [n for n in out["notes"] if isinstance(n, int) and 0 <= n <= 4][:8]
    return (out["title"], notes) if len(notes) >= 4 else None


async def ai_diary():
    since = time.time() - 86400
    evs = db.execute("select * from events where at > ? order by at", (since,)).fetchall()
    lines = [f"{datetime.fromtimestamp(e['at'], TZ):%H:%M} {e['who']}: {e['what']}" for e in evs] or ["(a quiet day)"]
    return await ask(PERSONA, "Write today's diary entry: exactly three short, funny lines from your point of view, "
                     "then the same three lines in Japanese. Today's events:\n" + "\n".join(lines[-60:]) +
                     f"\nYour state now: {state_line(device['state'])}", effort="low", max_tokens=800)


async def ai_haiku():
    return await ask(PERSONA, "Write one haiku about your life as a pocket cockatiel (today's mood: "
                     f"{device['state'].get('mood', 'content')}). First in Japanese (5-7-5), then an English "
                     "version. No title.", effort="low", max_tokens=400)

# ---------------------------------------------------------------------------------------- telegram

session: aiohttp.ClientSession = None


async def tg(method, **params):
    files = {k: v for k, v in params.items() if isinstance(v, (bytes, tuple))}
    try:
        if files:
            form = aiohttp.FormData()
            for k, v in params.items():
                if k in files:
                    data, fname = v if isinstance(v, tuple) else (v, "okame.png")
                    form.add_field(k, data, filename=fname)
                elif v is not None:
                    form.add_field(k, v if isinstance(v, str) else json.dumps(v))
            async with session.post(f"{API}/{method}", data=form) as r:
                return await r.json()
        async with session.post(f"{API}/{method}", json={k: v for k, v in params.items() if v is not None}) as r:
            return await r.json()
    except aiohttp.ClientError as e:
        log.warning("telegram %s failed: %s", method, e)
        return {}


async def say(chat_id, text):
    await tg("sendMessage", chat_id=chat_id, text=text, parse_mode="HTML")


async def show(chat_id, caption, frame=None, label=None):
    await tg("sendPhoto", chat_id=chat_id, photo=(lcd_png(frame or device["frame"], label), "okame.png"),
             caption=caption, parse_mode="HTML")


def hearts(n):
    return "💛" * int(n or 0) + "🤍" * (4 - int(n or 0))


def esc(s):
    return (s or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def online():
    return time.time() - device["last_seen"] < 45


def status_caption():
    s = device["state"]
    if not s:
        return "🥚 I haven't hatched into this world yet… (my screen hasn't checked in yet)."
    seen = "awake on Wi-Fi ✨" if online() else \
        f"napping away from Wi-Fi 💤 (last seen {datetime.fromtimestamp(device['last_seen'], TZ):%a %H:%M})"
    name = esc(s.get("name")) or "Okame"
    return (f"<b>{name}</b> · {s.get('stage')} · {s.get('ageDays', 0)}d · {s.get('weight', 0)}g\n"
            f"Food {hearts(s.get('hunger'))}\nJoy  {hearts(s.get('happy'))}\n"
            f"Mood: {s.get('mood')}{' · 🤒 sick' if s.get('sick') else ''}{' · 💩' * int(s.get('poop') or 0)}\n{seen}")


CARE = {"feed": ("seed", "Crunch crunch 🌻"), "treat": ("millet", "Millet!! My favourite 🌾"),
        "bath": ("bath", "Splish splash 🛁"), "vet": ("vet", "Off to the vet 🩺"),
        "scritch": ("scritch", "Head scritches… *melts* 🥰"), "cover": ("light", "Lights… 🌙")}
RESULT_TEXT = {"refused": "…but it isn't hungry right now 🙅", "not-needed": "…not needed, all good! ✨",
               "sleeping": "…shh, the cage is covered. Uncover it first 🌙"}
HELP = ("<b>オカメ Okame</b>, your pocket cockatiel 🐦💛\n\n"
        "<b>Look after me</b>\n/status how I'm doing · /photo my screen\n/feed /treat /bath /vet /scritch /cover\n\n"
        "<b>Make my screen sparkle</b>\n/note &lt;text&gt; a love note on my screen 💌\n/hug a big hug on my screen 🤗\n/home &lt;date&gt; count down the days until you're home 🚢\n"
        "/teach &lt;word&gt; teach me to say something 🦜 · /words\n"
        "/catch play Feather Catch against Haruki's Seed Rain score 🌻\n"
        "/draw &lt;idea&gt; I draw a pixel picture 🎨\n/hearts /fireworks /sakura /rain ✨\n\n"
        "<b>Just for fun</b>\n/song &lt;idea&gt; I compose a whistle 🎶\n/diary my day · /haiku a poem\n"
        "…or simply chat with me!")


async def handle(msg):
    chat = msg["chat"]["id"]
    frm = msg.get("from", {})
    name = frm.get("first_name", "friend")
    lang = (frm.get("language_code") or "en")[:2]
    text = (msg.get("text") or "").strip()
    u = user(chat)
    if not u:
        return await pairing(chat, name, lang, text)
    cmd, _, arg = text.partition(" ")
    cmd = cmd.lower().split("@")[0]
    arg = arg.strip()

    if cmd in ("/start", "/help"):
        return await say(chat, HELP)
    if cmd == "/status":
        return await show(chat, status_caption())
    if cmd == "/photo":
        return await show(chat, "📸 Cheese! ピヨ", label=(device["state"].get("name") or "OKAME"))
    if cmd.lstrip("/") in CARE:
        action, line = CARE[cmd.lstrip("/")]
        enqueue("act", {"action": action, "by": name}, chat)
        event(name, f"did {action}")
        return await say(chat, f"{line}{'' if online() else ' (I will get it as soon as I wake up 💤)'}")
    if cmd in ("/teach", "/words", "/forget"):
        return await words_cmd(chat, name, cmd, arg)
    if cmd == "/catch":
        return await catch_start(chat, name)
    if cmd == "/home":
        return await home_cmd(chat, name, arg)
    if cmd == "/hug":
        if not egg_has(V04):
            return await say(chat, UPDATE_FIRST)
        enqueue("hug", {"from": (to_lcd_text(name).replace("\x01", "").split() or ["FRIEND"])[0][:10]}, chat)
        event(name, "sent a hug")
        where = "right now" if online() else "the moment I'm back on Wi-Fi"
        return await say(chat, f"🤗 A big hug is on its way to my screen {where}!")
    if cmd == "/note":
        if not arg:
            return await say(chat, "Write it after the command, e.g. <code>/note I miss you ♥</code>")
        tr = await ai_translate(arg, lang)
        lcd = to_lcd_text(arg)
        if not lcd.replace("\x01", "").strip() and tr:
            lcd = to_lcd_text(tr)  # Japanese note: the screen shows the English translation
        enqueue("msg", {"from": name, "text": arg, "translation": tr or "", "lcd": lcd or "\x01"}, chat)
        event(name, f"sent a note: {arg[:60]}")
        where = "on my screen right now" if online() else "on my screen the moment I'm back on Wi-Fi"
        return await say(chat, f"💌 Got it! It will pop up {where}." + (f"\n<i>{esc(tr)}</i>" if tr else ""))
    if cmd in ("/hearts", "/fireworks", "/sakura", "/rain"):
        frames = anim_frames(cmd[1:])
        enqueue("pic", {"frames": frames, "ms": 300, "secs": 8, "by": name}, chat)
        event(name, f"sent {cmd[1:]}")
        return await tg("sendAnimation", chat_id=chat, animation=(preview_gif(frames), "okame.gif"),
                        caption=f"✨ {cmd[1:]} on my screen!")
    if cmd == "/draw":
        if not arg:
            return await say(chat, "What should I draw? e.g. <code>/draw a heart with wings</code>")
        await tg("sendChatAction", chat_id=chat, action="upload_photo")
        frame, title = await ai_draw(arg)
        if not frame:
            return await say(chat, "My crayon broke… try again in a bit 🖍️")
        enqueue("pic", {"frames": [frame], "ms": 1000, "secs": 12, "by": name}, chat)
        event(name, f"asked me to draw {arg[:40]}")
        return await show(chat, f"🎨 <b>{esc(title)}</b>, now on my screen!", frame=frame, label=title)
    if cmd == "/song":
        await tg("sendChatAction", chat_id=chat, action="typing")
        song = await ai_song(arg or "a happy day together")
        if not song:
            return await say(chat, "Hmm, no tune came to me… ピヨ? Try again!")
        title, notes = song
        enqueue("tune", {"notes": "".join(map(str, notes)), "title": title, "by": name}, chat)
        event(name, f"gave me a song: {title}")
        sol = " ".join(["do", "re", "mi", "so", "la"][n] for n in notes)
        return await say(chat, f"🎶 <b>{esc(title)}</b>\n<code>{sol}</code>\nI'm learning it! Practise it with me on "
                               "the phone page, once a day for three days.")
    if cmd == "/diary":
        await tg("sendChatAction", chat_id=chat, action="typing")
        return await say(chat, esc(await ai_diary() or "Dear diary… I forgot what happened. ピヨ"))
    if cmd == "/haiku":
        await tg("sendChatAction", chat_id=chat, action="typing")
        return await say(chat, esc(await ai_haiku() or "ピヨピヨ…"))
    if text.startswith("/"):
        return await say(chat, "I don't know that trick yet 🐦 /help")
    if text:
        await tg("sendChatAction", chat_id=chat, action="typing")
        reply = await ai_chat(name, text)
        event(name, f"chatted: {text[:40]}")
        return await say(chat, esc(reply or "ピヨ? *tilts head*"))


async def pairing(chat, name, lang, text):
    p = db.execute("select * from pairing where chat_id=?", (chat,)).fetchone()
    if p and re.fullmatch(r"\d{4}", text) and time.time() < p["expires"]:
        if hmac.compare_digest(text, p["code"]):
            db.execute("insert or replace into users values(?,?,?,?)", (chat, name, lang, time.time()))
            db.execute("delete from pairing where chat_id=?", (chat,))
            enqueue("pairdone", {}, chat)
            event(name, "became my friend on Telegram")
            await show(chat, f"🎉 We're friends now, {esc(name)}! ピヨピヨ\n\n" + HELP)
            return
        tries = p["tries"] + 1
        if tries >= 5:
            db.execute("delete from pairing where chat_id=?", (chat,))
            return await say(chat, "Too many wrong codes 🙈 Send /start for a new one.")
        db.execute("update pairing set tries=? where chat_id=?", (tries, chat))
        return await say(chat, "That's not the code on my screen 🤔 Try again?")
    # A one-time invite link (t.me/<bot>?start=<token>) skips the screen code: an admin puts {"token", "until"} in kv "invite".
    inv = kv_get("invite", {})
    if inv and time.time() < inv["until"] and hmac.compare_digest(text.encode(), f"/start {inv['token']}".encode()):
        kv_set("invite", {})
        db.execute("insert or replace into users values(?,?,?,?)", (chat, name, lang, time.time()))
        enqueue("pairdone", {}, chat)
        event(name, "became my friend on Telegram")
        return await show(chat, f"🎉 We're friends now, {esc(name)}! ピヨピヨ\n\n" + HELP)
    if len(users()) >= 6:
        return await say(chat, "ピヨ? I only talk to my family 🐦")
    last = kv_get(f"pairask:{chat}", 0)
    if time.time() - last < 30:
        return await say(chat, "Look at my screen for the code 👀")
    kv_set(f"pairask:{chat}", time.time())
    code = f"{random.SystemRandom().randrange(10000):04d}"
    db.execute("insert or replace into pairing values(?,?,?,?)", (chat, code, time.time() + 900, 0))
    enqueue("pair", {"code": code}, chat)
    when = "Look at my screen now" if online() else "I'm napping away from Wi-Fi; the code appears when I wake up"
    await say(chat, f"Hi {esc(name)}! ピヨ 🐦\nTo be friends, type the <b>4-digit code</b> on my little screen. "
                    f"{when} 👀")


async def telegram_loop():
    offset = kv_get("tg_offset", 0)
    await tg("setMyCommands", commands=[
        {"command": "status", "description": "How is Okame doing?"},
        {"command": "photo", "description": "A picture of Okame's screen"},
        {"command": "feed", "description": "Seed 🌻"}, {"command": "treat", "description": "Millet treat 🌾"},
        {"command": "scritch", "description": "Head scritches 🥰"}, {"command": "bath", "description": "Clean the cage 🛁"},
        {"command": "vet", "description": "Visit the vet 🩺"}, {"command": "cover", "description": "Cover / uncover the cage 🌙"},
        {"command": "note", "description": "A love note on Okame's screen 💌"},
        {"command": "hug", "description": "A big hug on Okame's screen 🤗"},
        {"command": "home", "description": "Countdown to coming home 🚢"},
        {"command": "teach", "description": "Teach Okame a word 🦜"},
        {"command": "words", "description": "Words Okame can say"},
        {"command": "catch", "description": "Feather Catch: beat Haruki's score 🌻"},
        {"command": "draw", "description": "Okame draws a pixel picture 🎨"},
        {"command": "hearts", "description": "Hearts on the screen 💛"}, {"command": "fireworks", "description": "Fireworks ✨"},
        {"command": "sakura", "description": "Falling sakura 🌸"}, {"command": "song", "description": "Okame composes a whistle 🎶"},
        {"command": "diary", "description": "Okame's diary 📔"}, {"command": "haiku", "description": "A haiku 🍃"},
        {"command": "help", "description": "Everything Okame can do"}])
    await tg("setMyShortDescription", short_description="Okame オカメ, a pocket cockatiel living on a little pixel screen 🐦💛")
    await tg("setMyDescription", description="ピヨ! I'm Okame, a tiny cockatiel who lives on a little screen next to "
             "Haruki's laptop. Feed me, play with me and send notes that pop up on my screen. Press Start to become "
             "my friend (you'll need the code on my screen).")
    while True:
        try:
            async with session.get(f"{API}/getUpdates", params={"offset": offset, "timeout": 50},
                                   timeout=aiohttp.ClientTimeout(total=70)) as r:
                data = await r.json()
        except (aiohttp.ClientError, asyncio.TimeoutError):
            await asyncio.sleep(3)
            continue
        for upd in data.get("result", []):
            offset = upd["update_id"] + 1
            kv_set("tg_offset", offset)
            if "message" in upd:
                asyncio.create_task(safe(handle(upd["message"])))
            elif "callback_query" in upd:
                asyncio.create_task(safe(on_callback(upd["callback_query"])))


async def safe(coro):
    try:
        await coro
    except Exception:
        log.exception("handler failed")

# ---------------------------------------------------------------------------------------- egg API


def pending():
    # A delivered command is only sent again when the egg hasn't confirmed it after 5 minutes (it may have rebooted).
    rows = db.execute("select * from queue where done is null and (delivered is null or delivered < ?) "
                      "order by id limit 8", (time.time() - 300,)).fetchall()
    return [{"id": r["id"], "type": r["kind"], **json.loads(r["payload"])} for r in rows]


async def sync(request):
    if not hmac.compare_digest(request.headers.get("X-Okame-Key", ""), CFG["device_key"]):
        return web.json_response({"error": "nope"}, status=401)
    body = await request.json()
    away = time.time() - device["last_seen"]
    device["last_seen"] = time.time()
    fw_before = device["state"].get("fw")
    if body.get("state"):
        before = device["state"]
        device["state"] = body["state"]
        kv_set("state", body["state"])
        await notice_changes(before, body["state"])
    if body.get("frame"):
        device["frame"] = body["frame"]
        kv_set("frame", body["frame"])
    for ack in body.get("acks", []):
        await acked(ack)
    for e in body.get("events", [])[:16]:
        if isinstance(e, dict):
            asyncio.create_task(safe(on_event(e)))
    if away > 300 or device["state"].get("fw") != fw_before:
        asyncio.create_task(safe(on_connect()))
    cmds = pending()
    if not cmds and body.get("wait"):
        new_command.clear()
        try:
            await asyncio.wait_for(new_command.wait(), timeout=min(int(body["wait"]), 20))
        except asyncio.TimeoutError:
            pass
        cmds = pending()
    for c in cmds:
        db.execute("update queue set delivered=? where id=?", (time.time(), c["id"]))
    return web.json_response({"commands": cmds, "now": int(time.time())})


async def acked(ack):
    row = db.execute("select * from queue where id=? and done is null", (ack.get("id"),)).fetchone()
    if not row:
        return
    result = str(ack.get("result", "ok"))[:40]
    db.execute("update queue set done=?, result=? where id=?", (time.time(), result, row["id"]))
    chat, kind = row["chat_id"], row["kind"]
    if not chat:
        return
    if kind == "msg":
        await say(chat, "✓ Your note is on my screen now 💌")
    elif kind == "hug" and result == "shown":
        await say(chat, "🤗 Your hug just arrived on my screen. ピヨ💛")
    elif kind == "act":
        payload = json.loads(row["payload"])
        await asyncio.sleep(2.5)  # let the egg's animation start before taking its picture
        await show(chat, RESULT_TEXT.get(result, "Done! ピヨ ✨"), label=payload["action"])


def fw_tuple(s):
    return tuple(int(x) for x in re.findall(r"\d+", (s or "0").split("-")[0])[:3]) or (0,)


def egg_has(version):
    """True when the egg runs firmware `version` or newer (it reports its version on every sync)."""
    return fw_tuple(device["state"].get("fw")) >= version


V04 = (0, 4, 0)  # hug, words, countdown, weather, dates, rival: older eggs answer "unknown" to these
UPDATE_FIRST = "My little screen needs an update before I can do that 🛠️ ピヨ"


def partners():
    """Who hears about Haruki's taps on the page (💛, scores): Matthieu's chat, from config.json."""
    ids = set(CFG.get("partner_chats", []))
    return [u for u in users() if u["chat_id"] in ids]


async def on_event(e):
    """Something happened on the egg (cloud::event). Handlers are added per feature in EVENT_HANDLERS."""
    handler = EVENT_HANDLERS.get(str(e.get("type")))
    if handler:
        await handler(e)
    else:
        log.info("egg event %s (no handler)", e.get("type"))


async def on_connect():
    """The egg is back after a while (or was updated): resend what it keeps only in memory, and its settings."""
    if not egg_has(V04):
        return
    for fn in CONNECT_HOOKS:
        await fn()


async def love_tapped(e):
    """Haruki tapped 💛 on the phone page."""
    if time.time() - kv_get("last_love", 0) < 60:
        return
    kv_set("last_love", time.time())
    event("Haruki", "sent a 💛 from the page")
    for p in partners():
        await show(p["chat_id"], "💛 Haruki is thinking of you right now. Send one back with /hug 🤗")


def parse_day(text, today=None):
    """'2026-12-15', '15/12/2026', '15-12' or '15.12' (the next one coming) -> date, else None."""
    today = today or datetime.now(TZ).date()
    t = text.strip()
    try:
        if m := re.fullmatch(r"(\d{4})-(\d{1,2})-(\d{1,2})", t):
            return date(int(m[1]), int(m[2]), int(m[3]))
        if m := re.fullmatch(r"(\d{1,2})[/.-](\d{1,2})(?:[/.-](\d{2,4}))?", t):
            y = int(m[3]) if m[3] else today.year
            y += 2000 if y < 100 else 0
            d = date(y, int(m[2]), int(m[1]))
            return d if m[3] or d >= today else date(y + 1, d.month, d.day)
    except ValueError:
        return None
    return None


def home_days():
    h = kv_get("home")
    return (date.fromisoformat(h) - datetime.now(TZ).date()).days if h else None


async def send_home():
    h = kv_get("home")
    d = date.fromisoformat(h) if h else None
    enqueue("home", {"y": d.year, "m": d.month, "d": d.day} if d else {"y": 0, "m": 0, "d": 0})


async def home_cmd(chat, name, arg):
    if not egg_has(V04):
        return await say(chat, UPDATE_FIRST)
    if not arg:
        n = home_days()
        if n is None or n < 0:
            return await say(chat, "When are you coming home? e.g. <code>/home 15/12</code> 🚢")
        return await say(chat, f"🚢 {n} day{'s' * (n != 1)} to go! (Change it with <code>/home 15/12</code>, "
                               f"or <code>/home off</code>.)")
    if arg.lower() in ("off", "clear", "none", "stop"):
        db.execute("delete from kv where k='home'")
        await send_home()
        return await say(chat, "OK, no countdown on my screen for now.")
    d = parse_day(arg)
    if not d or d < datetime.now(TZ).date():
        return await say(chat, "I couldn't read that date 🤔 Try <code>/home 15/12</code> or <code>/home 2026-12-15</code>.")
    kv_set("home", d.isoformat())
    await send_home()
    event(name, f"set the homecoming to {d.isoformat()}")
    n = (d - datetime.now(TZ).date()).days
    return await say(chat, f"🚢 Countdown on! {n} day{'s' * (n != 1)} until you're home. "
                           f"I'll cross one off every morning ✏️")


async def lcd_word(text):
    """A word the screen can show: Latin letters as they are; Japanese (or anything else) as Hepburn romaji."""
    lcd = to_lcd_text(text).replace("\x01", "").strip()
    if len(lcd) < len(re.sub(r"\s", "", text)) // 2 or not lcd:
        romaji = await ask("Convert the user's word or short phrase to Hepburn romaji. Reply with the romaji only, "
                           "plain ASCII letters and spaces, no punctuation.", text, effort="low", max_tokens=60)
        lcd = to_lcd_text(romaji or "")
    return lcd[:24].strip()


async def words_cmd(chat, name, cmd, arg):
    if not egg_has(V04):
        return await say(chat, UPDATE_FIRST)
    words = kv_get("words", [])
    if cmd == "/words":
        if not words:
            return await say(chat, "I don't know any words yet. Teach me one: <code>/teach おやすみ</code> 🦜")
        return await say(chat, "🦜 Words I can say:\n" + "\n".join(f"• {esc(w)}" for w in words) +
                         "\n\nForget one with <code>/forget WORD</code>.")
    if not arg:
        return await say(chat, f"Which word? e.g. <code>{cmd} {'おやすみ' if cmd == '/teach' else 'OYASUMI'}</code>")
    word = await lcd_word(arg)
    if not word:
        return await say(chat, "I can't say that one 🙈 My screen only knows Latin letters; try writing it in romaji, "
                               "e.g. <code>/teach oyasumi</code>.")
    if cmd == "/forget":
        if word not in words:
            return await say(chat, f"I don't know {esc(word)} 🤔 /words shows what I know.")
        kv_set("words", [w for w in words if w != word])
        enqueue("forget", {"text": word}, chat)
        return await say(chat, f"OK, I'll forget {esc(word)}.")
    if word in words:
        return await say(chat, f"I already know {esc(word)}! ピヨ")
    kv_set("words", (words + [word])[-8:])
    enqueue("word", {"text": word}, chat)
    event(name, f"taught me to say {word}")
    return await say(chat, f"🦜 {esc(word)}… {esc(word[:max(1, (len(word) + 1) // 2)])}..? I'm practising! "
                           "Give me a day and I'll say it properly, now and then, on my screen.")


async def resend_words():
    """A new egg forgets its words; teach it again what Matthieu taught the last one."""
    words = kv_get("words", [])
    if words and device["state"].get("words", 0) < len(words):
        for w in words:
            enqueue("word", {"text": w})


OMIKUJI = ["大吉", "中吉", "小吉", "吉", "末吉", "凶"]


async def omikuji_drawn(e):
    """Haruki drew today's fortune. The two that matter most reach Matthieu: 大吉 to celebrate, 凶 to cheer her up."""
    g = int(e.get("grade", 3))
    event("Haruki", f"drew {OMIKUJI[g % 6]} at the omikuji")
    if g == 0:
        text = "🎋 Haruki drew 大吉 (great blessing) at my omikuji today! Lucky day ✨"
    elif g == 5:
        text = "🎋 Haruki drew 凶 (bad luck) at my omikuji today… maybe send her a /hug? 🤗"
    else:
        return
    for p in partners():
        await say(p["chat_id"], text)


def classify_weather(code, temp):
    """open-meteo WMO code + °C -> what the bird reacts to."""
    if code in (71, 73, 75, 77, 85, 86):
        return "snow"
    if 51 <= code <= 67 or 80 <= code <= 82 or code >= 95:
        return "rain"
    if temp >= 28:
        return "hot"
    if temp <= 3:
        return "cold"
    return "clear"


async def fetch_weather():
    w = CFG.get("weather")
    if not w:
        return None
    url = ("https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}"
           "&current=temperature_2m,weather_code&timezone=auto").format(**w)
    try:
        async with session.get(url, timeout=aiohttp.ClientTimeout(total=15)) as resp:
            cur = (await resp.json())["current"]
    except (aiohttp.ClientError, asyncio.TimeoutError, KeyError, ValueError):
        return None
    temp = round(cur["temperature_2m"])
    return {"kind": classify_weather(int(cur["weather_code"]), temp), "temp": temp, "place": w.get("place", "")}


async def send_weather(force=False):
    if not egg_has(V04):
        return
    wx = kv_get("wx")
    if wx and (force or time.time() - kv_get("wx_sent", 0) > 2 * 3600 or wx != kv_get("wx_last_sent")):
        enqueue("wx", wx)
        kv_set("wx_sent", time.time())
        kv_set("wx_last_sent", wx)


async def weather_loop():
    while True:
        wx = await fetch_weather()
        if wx:
            kv_set("wx", wx)
            await send_weather()
        await asyncio.sleep(30 * 60)


async def send_dates():
    ds = CFG.get("dates", [])
    who = next((d.get("who", "") for d in ds if d.get("kind") == 1), "")
    enqueue("dates", {"list": [{"m": d["m"], "d": d["d"], "kind": d["kind"]} for d in ds][:4],
                      "who": to_lcd_text(who)[:12]})


def special_today(day):
    return [d for d in CFG.get("dates", []) if d.get("m") == day.month and d.get("d") == day.day]


async def eve_reminders(tomorrow):
    """The evening before a birthday: a heads-up for Matthieu, with time to send a note for the morning."""
    for d in special_today(tomorrow):
        if d.get("kind") == 1:
            age = f" ({tomorrow.year - d['year']})" if d.get("year") else ""
            for p in partners():
                await say(p["chat_id"], f"🎂 Psst… tomorrow is {esc(d.get('who', ''))}'s birthday{age}! I'll throw a "
                                        "party on her screen in the morning. Send a /note tonight so it's waiting there 💌")


async def birthdays(now):
    """On the day: wishes for everyone (Matthieu's heads-up comes the evening before, see eve_reminders)."""
    if (now.month, now.day) == (7, 7):
        for u in users():
            await say(u["chat_id"], "🎋 Today is Tanabata. Orihime and Hikoboshi, two stars kept apart by the Milky Way, "
                                    "cross it to meet tonight, once a year. Some couples are kept apart by a sea… "
                                    "but they get a lot more than once a year 💛 Look at my screen ✨")
    for d in special_today(now.date()):
        if d.get("kind") == 1:
            for u in users():
                await show(u["chat_id"], f"🎂🎉 Happy birthday, {esc(d.get('who', ''))}! ピヨピヨ♪ "
                                         "There's cake on my screen today!")


async def focus_done(e):
    m = int(e.get("minutes", 25))
    event("Haruki", f"focused for {m} minutes with me")
    for p in partners():
        await say(p["chat_id"], f"📚 Haruki just finished {m} minutes of focus with me. がんばったね！ "
                                "A /note would make her break even better 💌")


# ---------------------------------------------------------------------------------------- Feather Catch
# Matthieu's side of the Seed Rain duel, played with inline buttons: a seed appears in one of five slots; tap it
# within the window. Same scoring as Seed Rain (golden = 3, three lives), about as long (15 rounds of up to 3 s).

# The window is generous because a ship's satellite link can take a second or two each way.
CATCH_ROUNDS, CATCH_LIVES, CATCH_WINDOW, CATCH_GOLDEN = 15, 3, 4.5, 0.15
catches = {}  # chat id -> the game in progress


def catch_new_round(g):
    g["col"] = random.randrange(5)
    g["golden"] = random.random() < CATCH_GOLDEN
    g["shown"] = time.time()
    g["busy"] = False


def catch_board(g):
    seed = "⭐" if g["golden"] else "🌻"
    return {"inline_keyboard": [[{"text": seed if i == g["col"] else "·",
                                  "callback_data": f"catch:{g['id']}:{g['round']}:{i}"} for i in range(5)]]}


def catch_text(g, note=""):
    hearts = "❤️" * g["lives"] + "🤍" * (CATCH_LIVES - g["lives"])
    return (f"🌻 <b>Feather Catch</b> · {g['round'] + 1}/{CATCH_ROUNDS}\nScore <b>{g['score']}</b> · {hearts}\n"
            f"Tap the seed before it falls! ⭐ = 3" + (f"\n{note}" if note else ""))


async def catch_start(chat, name):
    g = {"id": os.urandom(3).hex(), "round": 0, "score": 0, "lives": CATCH_LIVES, "chat": chat, "name": name}
    catch_new_round(g)
    res = await tg("sendMessage", chat_id=chat, text=catch_text(g), parse_mode="HTML", reply_markup=catch_board(g))
    g["msg"] = (res.get("result") or {}).get("message_id")
    g["shown"] = time.time()  # the clock starts once the board is on its way to his phone
    catches[chat] = g
    asyncio.create_task(safe(catch_timeout(g, 0)))


async def catch_timeout(g, rnd):
    await asyncio.sleep(CATCH_WINDOW + 0.3)  # a little grace for a slow link
    if catches.get(g["chat"]) is g and g["round"] == rnd and not g["busy"]:
        await catch_resolve(g, False, "💨 Too slow, it fell!")


async def on_callback(cq):
    arrived = time.time()  # judge the tap by when it reached us, not after our own reply round-trip
    asyncio.create_task(safe(tg("answerCallbackQuery", callback_query_id=cq["id"])))  # don't let the reply delay it
    parts = (cq.get("data") or "").split(":")
    if len(parts) != 4 or parts[0] != "catch":
        return
    g = catches.get((cq.get("message") or {}).get("chat", {}).get("id"))
    if not g or g["id"] != parts[1] or int(parts[2]) != g["round"] or g["busy"]:
        return
    late = arrived - g["shown"] > CATCH_WINDOW + 0.3
    hit = int(parts[3]) == g["col"] and not late
    await catch_resolve(g, hit, "💨 Too slow!" if late else "")


async def catch_resolve(g, hit, note=""):
    g["busy"] = True
    if hit:
        g["score"] += 3 if g["golden"] else 1
        note = "⭐ Golden! +3" if g["golden"] else "🌻 Caught it! +1"
    else:
        g["lives"] -= 1
        note = note or "🙈 Missed!"
    g["round"] += 1
    if g["round"] >= CATCH_ROUNDS or g["lives"] <= 0:
        return await catch_end(g, note)
    catch_new_round(g)
    await tg("editMessageText", chat_id=g["chat"], message_id=g["msg"], text=catch_text(g, note), parse_mode="HTML",
             reply_markup=catch_board(g))
    g["shown"] = time.time()
    asyncio.create_task(safe(catch_timeout(g, g["round"])))


async def catch_end(g, note):
    catches.pop(g["chat"], None)
    score, chat = g["score"], g["chat"]
    her = int(device["state"].get("bestRain") or 0)
    mine = chat in {p["chat_id"] for p in partners()}
    best = kv_get("catch_best", 0)
    lines = [note, f"🏁 <b>Game over!</b> You caught <b>{score}</b>."]
    if mine and score > best:
        kv_set("catch_best", score)
        lines.append("🏆 New personal best!")
        if egg_has(V04):
            enqueue("rival", {"score": score})
        for u in users():
            if u["chat_id"] != chat:
                await say(u["chat_id"], f"🌻 {esc(g['name'])} just scored {score} in Feather Catch! "
                                        "Beat it in Seed Rain on my page 😤🌧️")
    if mine and her:
        lines.append(f"Haruki's Seed Rain best is {her}: " +
                     ("you're ahead! 😎" if max(score, best) > her else "she's still ahead! 💪" if her > max(score, best)
                      else "it's a tie! 🤝"))
    lines.append("Again? /catch")
    await tg("editMessageText", chat_id=chat, message_id=g["msg"], text="\n".join(lines), parse_mode="HTML")
    event(g["name"], f"scored {score} in Feather Catch")


async def rain_record(e):
    """Haruki set a new Seed Rain best on the egg."""
    score = int(e.get("score", 0))
    event("Haruki", f"set a Seed Rain record: {score}")
    rival = kv_get("catch_best", 0)
    for p in partners():
        beat = f" That beats your {rival} in Feather Catch! 😱" if rival and score > rival else ""
        await say(p["chat_id"], f"🌧️ Haruki set a new Seed Rain record: <b>{score}</b>!{beat} /catch to answer 😤")


async def send_rival():
    best = kv_get("catch_best", 0)
    if best:
        enqueue("rival", {"score": best})


EVENT_HANDLERS = {"love": love_tapped, "omikuji": omikuji_drawn, "focus": focus_done, "rain": rain_record}
async def weather_now():
    await send_weather(force=True)


WHATS_NEW_MATTHIEU = (
    "✨ <b>I just updated myself!</b> ピヨ New things you can do from the ship:\n\n"
    "🤗 /hug a big hug on my screen\n🚢 /home 15/12 a countdown to your return, crossed off every morning\n"
    "🦜 /teach おやすみ teach me a word; I'll say it now and then (after a day of practice)\n"
    "🌻 /catch Feather Catch, your side of a duel with Haruki's Seed Rain\n\n"
    "And you'll hear from me when Haruki taps 💛 on my page, draws 大吉 or 凶 at the omikuji, "
    "finishes a focus session or sets a Seed Rain record.")
WHATS_NEW_HARUKI = (
    "✨ <b>I just updated myself!</b> ピヨ New on my page:\n\n"
    "💛 a heart button that tells Matthieu you're thinking of him\n✊ Janken and 💃 Copy the dance\n"
    "🎋 a daily omikuji (collect all six!)\n📚 Focus with Okame, 25 or 50 minutes\n"
    "and I now know the weather outside, the seasons… and a certain birthday 🎂")


async def whats_new():
    if kv_get("whats_new_0.4"):
        return
    kv_set("whats_new_0.4", time.time())
    ids = {p["chat_id"] for p in partners()}
    for u in users():
        await say(u["chat_id"], WHATS_NEW_MATTHIEU if u["chat_id"] in ids else WHATS_NEW_HARUKI)


CONNECT_HOOKS = [send_home, resend_words, send_dates, weather_now, send_rival, whats_new]


async def notice_changes(before, after):
    """Little proactive messages: hatching, and a gentle nudge when the bird needs care (never at night)."""
    if before.get("stage") == "egg" and after.get("stage") == "chick":
        for u in users():
            await show(u["chat_id"], "🐣 I HATCHED! Hello world! ピヨピヨ")
    hour = datetime.now(TZ).hour
    if after.get("attention") and 9 <= hour < 21 and time.time() - kv_get("last_nudge", 0) > 4 * 3600:
        kv_set("last_nudge", time.time())
        need = "hungry 🥺" if after.get("hunger") == 0 else "a bit sad 🥺" if after.get("happy") == 0 else \
            "in need of a bath 💩" if (after.get("poop") or 0) >= 2 else "not feeling well 🤒" if after.get("sick") else \
            "needing a little attention 🥺"
        for u in users():
            await show(u["chat_id"], f"ピヨ… I'm {need}")


async def mornings():
    while True:
        now = datetime.now(TZ)
        if now.hour == 8 and kv_get("morning", "") != now.date().isoformat():
            kv_set("morning", now.date().isoformat())
            n = home_days()
            partner_ids = {p["chat_id"] for p in partners()}
            for u in users():
                me = u["chat_id"] in partner_ids
                extra = (f"\n🏠 {'You come' if me else 'Matthieu comes'} home TODAY! 🎉" if n == 0 else
                         f"\n🚢 {n} day{'s' * (n != 1)} until {'you are' if me else 'Matthieu is'} home"
                         if n and n > 0 else "")
                greet = "おはよう！" if u["lang"] == "ja" else "Goedemorgen!" if u["lang"] == "nl" else "Good morning!"
                await show(u["chat_id"], f"☀️ {greet} ピヨ\n" + status_caption() + extra)
            await birthdays(now)
        if now.hour == 20 and kv_get("evening", "") != now.date().isoformat():
            kv_set("evening", now.date().isoformat())
            from datetime import timedelta
            await eve_reminders(now.date() + timedelta(days=1))
        await asyncio.sleep(60)


async def health(request):
    return web.json_response({"ok": True, "egg_online": online()})


async def claim(request):
    # One-time handover of the device key to a reflashed egg: an admin puts {"token", "until"} in kv "claim".
    c = kv_get("claim", {})
    if not c or time.time() > c["until"] or not hmac.compare_digest(request.match_info["token"], c["token"]):
        return web.Response(status=404)
    kv_set("claim", {})
    log.info("device key handed over by claim link")
    return web.Response(text=CFG["device_key"])


async def main():
    global session
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    session = aiohttp.ClientSession()
    app = web.Application(client_max_size=64 * 1024)
    app.router.add_post("/v1/sync", sync)
    app.router.add_get("/v1/health", health)
    app.router.add_get("/v1/claim/{token}", claim)
    ctx = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
    ctx.load_cert_chain(f"{HOME}/relay.crt", f"{HOME}/relay.key")
    runner = web.AppRunner(app, access_log=None)
    await runner.setup()
    await web.TCPSite(runner, "0.0.0.0", CFG.get("port", 8443), ssl_context=ctx).start()
    log.info("okame relay up on :%s, AI %s", CFG.get("port", 8443), "on" if ai else "off")
    await asyncio.gather(telegram_loop(), mornings(), weather_loop())


if __name__ == "__main__":
    asyncio.run(main())

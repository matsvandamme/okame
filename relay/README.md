# Okame relay

A small Python service on a server you control. It is two things at once:

- **the Telegram bot** Matthieu and Haruki talk to (`/feed`, `/note`, `/photo`, chatting with Okame…), and
- **the egg's mailbox**: the dongle polls `POST /v1/sync` over TLS, sends its state and screen, and picks up
  commands. Notes sent while the dongle is offline wait here until it is back.

Nothing personal lives in this repo. Everything private (the chat ids, the weather location, the birthdays, the
device key, the tokens) lives in `/opt/okame` on the server.

## Files on the server

```
/opt/okame/
  app/okame_relay.py   the code (this folder's okame_relay.py)
  app/backup.sh        nightly database copy (cron)
  venv/                Python 3.14 with requirements.txt
  config.json          see config.example.json
  telegram_token       from @BotFather
  anthropic_key        for the AI persona (optional: without it Okame answers with canned lines)
  relay.crt relay.key  the TLS certificate the egg pins (src/relay_cert.h holds the public half)
  okame.db             sqlite: users, queue, events, kv
  backup/              rolling copies made by backup.sh
```

## Install

```sh
useradd -r -m -d /opt/okame okame
python3 -m venv /opt/okame/venv && /opt/okame/venv/bin/pip install -r requirements.txt
cp okame_relay.py backup.sh /opt/okame/app/
cp okame-relay.service /etc/systemd/system/ && systemctl enable --now okame-relay
```

Open the port in `config.json` (8443) on the firewall. Only that port: the egg speaks to nothing else.

## Day to day

- Deploy a change: `scp relay/okame_relay.py server:/opt/okame/app/ && ssh server systemctl restart okame-relay`
- Logs: `journalctl -u okame-relay -f`
- Health: `curl -sk https://127.0.0.1:8443/v1/health` → `{"ok": true, "egg_online": true}`
- Tests (stubbed Telegram, AI and egg): `uv run --with aiohttp --with anthropic --with pillow python relay/test_relay.py`

## How the egg gets its key

The device key never travels in the clear. `tools/claim_key.py` puts a one-time token in the relay's `kv`
(`claim`), and the dongle fetches the key once over the pinned TLS link with `GET /v1/claim/<token>`.

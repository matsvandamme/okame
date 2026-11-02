# Okame オカメ

A pocket cockatiel that lives on a USB dongle with a tiny screen, in the style of a 90s Japanese LCD virtual pet.
It was made as a gift, and it is built around one idea: **no buttons**. Everything happens from a phone page the
dongle serves itself (`okame.local`) and from a Telegram bot, so the person who got it never has to touch a
setting they don't want to.

- It sleeps until the dongle finds a saved Wi-Fi network, then wakes up and wants seed, baths, scritches and
  games. Away from Wi-Fi, time stands still (at most 12 hours are caught up on return).
- The bird grows from an egg into a chick, a juvenile and an adult whose feathers show how well it was cared
  for, becomes a wise old bird at 30 days, and can turn golden. Later generations inherit from their parents.
- Mini-games (whistle duet, peekaboo, Seed Rain, janken, copy the dance, Left or Right?), a daily omikuji, a
  focus timer, taught words and tunes, seasons, holidays and the weather outside, visiting bird friends, a bird
  shop paid in seeds, cage toys, outings with postcards, a family album, a clock and an alarm.
- A Telegram bot in the bird's voice (an AI persona) that relays notes, hugs, drawings and tunes to the screen,
  and tells the family what the bird is up to.
- Signed over-the-air updates from GitHub releases: the dongle only installs images signed with the release key.

See [CHANGELOG.md](CHANGELOG.md) for what each release added.

## Hardware

A "Pocket-Dongle-S3-0.96" (a LilyGO T-Dongle-S3 lookalike from AliExpress): ESP32-S3, 16 MB flash, 8 MB PSRAM,
a 0.96" 160×80 ST7735 screen, native USB. The firmware draws a 40×20 one-bit "LCD" on it with 3×3 dots and a
gap, so it looks like the real thing.

The clone's pins differ from LilyGO's documentation. This board uses SCLK 10, MOSI 11, CS 12, DC 13, RST 14 and
backlight on GPIO 38 (active high); see `src/display.h`. If yours is different, that file is the only place to
change. The BOOT button (GPIO 0) is not used: it sits where the owner can't reach it.

The partition table (`partitions.csv`) has two 4 MB app slots for OTA with rollback, plus a core-dump partition.

## Layout

```
lib/okame_core/   the pet rules, games and the dot-matrix UI: pure C++, no Arduino, shared by the firmware,
                  the desktop simulator and the native tests
src/              firmware: display, Wi-Fi + setup hotspot, the phone page API, the relay link, signed OTA
src/sim/          desktop simulator: renders every screen to PNG
web/index.html    the phone page, inlined and gzipped into the firmware by tools/embed_web.py
assets/sprites/   the pixel art as ASCII sheets, compiled to lib/okame_core/src/sprites.h by tools/gen_sprites.py
relay/            the Telegram bot + the egg's mailbox (Python, aiohttp); see relay/README.md
tools/            build hooks, the mock page server, screenshots, the release script
test/test_core/   native unit tests (PlatformIO + Unity)
case/ render/     a parametric CadQuery desk-egg case and Blender renders (optional)
```

## Build, flash, run

Needs [PlatformIO](https://platformio.org) (`pip install platformio` or `uv tool install platformio`) and Python 3.

```sh
pio run -e okame -t upload     # build and flash over USB
pio device monitor             # serial: type `state`, `cloud` or `screen` for diagnostics
```

`tools/pio_pre.py` only uploads to a dongle whose USB serial number matches `DONGLE_SERIAL`: this machine has
other ESP32 boards and PlatformIO must never guess. Put your dongle's serial there (it's the `SerialNumber` in
`pio device list`), or remove the guard.

First boot: with no saved network, after a minute the screen shows its own Wi-Fi name and password. Join it from
a phone, open the page it redirects to, add your home Wi-Fi. From then on the dongle wakes up on that network at
`http://okame.local` (the name can be changed from the page).

### Versions and updates

`VERSION` holds the firmware name and its OTA number (`0.7.1 9`). A USB build is `0.7.1-dev` with the same
number, so a dev flash is never replaced by the published release of the same number. The dongle checks GitHub
for a newer signed release three minutes after boot and once a day, or on request (`/update` in the bot, or
Settings on the page). A new image runs on trial until it has been online with a real clock for a minute;
until then a reset rolls back to the previous one.

To publish:

```sh
# once: the signing key, kept outside the repo (its public half is src/ota_pubkey.h)
openssl ecparam -name prime256v1 -genkey -noout -out ~/.okame/ota_ecdsa_p256.pem
# each release: bump VERSION and CHANGELOG.md, commit, push, then
uv run --with cryptography python tools/release.py --publish
```

The script refuses a dirty or unpushed tree and a version that isn't above the latest release, runs the tests,
builds with `OKAME_RELEASE=1`, signs `okame:<version>:<size>:<sha256>` and creates the GitHub release with the
notes from the changelog.

## Tests and checks

```sh
tools/check.sh          # everything CI runs: native tests, page routes, sprites in sync, relay tests, sim, firmware
tools/check.sh quick    # the same without the firmware build
```

- `pio test -e native` runs the core tests on your computer (the pet rules, games, screens, the save format).
- `python3 tools/check_routes.py` checks every API call in the page against the firmware's routes.
- `pio run -e sim && .pio/build/sim/program out/screens` renders every screen; `tools/contact_sheet.py` tiles
  them for review.
- The page without a dongle: `python3 tools/mock_server.py 8765 --named --full` and open `http://127.0.0.1:8765`
  (`--help` in the docstring lists the scenario flags: `--owl`, `--gone`, `--fuss`, `--away`, `--family`…).
  `tools/shoot_web.py` screenshots it at iPhone size with Playwright.

## Sprites

Each sprite is an ASCII block in `assets/sprites/*.txt`:

```
@ heart
.#.#.
#####
.###.
..#..
```

`#` ink, `o` forced clear, `w` a pattern fill (the wing: solid, pearl, pied or scruffy, chosen at draw time),
`.` transparent. `tools/gen_sprites.py` compiles them (the build hooks run it) and writes preview sheets to
`out/sprites/`.

## The relay and the bot

The dongle never talks to Telegram itself. It polls a small Python service over TLS (`POST /v1/sync`), sends
its state and screen, and picks up commands; the service is also the Telegram bot. Notes sent while the dongle
is offline wait there. Setup, files, backups: [relay/README.md](relay/README.md).

The dongle gets its relay key once over USB (`tools/claim_key.py`), and pins the relay's certificate
(`src/relay_cert.h`).

## Running your own

Everything personal lives outside the repo: on the relay server (`/opt/okame/config.json`) and in `~/.okame`.
To run a fork you change:

- `src/config.h`: `OKAME_REPO` (where your releases live) and `OKAME_TZ`;
- `src/ota_pubkey.h`: the public half of your own signing key;
- `src/cloud.cpp`: your relay's address, and `src/relay_cert.h` with its certificate;
- `tools/pio_pre.py`: your dongle's USB serial number;
- `tools/release.py`: `REPO`.

## License

MIT, see [LICENSE](LICENSE).

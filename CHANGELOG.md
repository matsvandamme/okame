# Changelog

Each heading is a firmware release (the OTA version number in brackets). `tools/release.py` uses the matching
section as the GitHub release notes. The relay is deployed separately and keeps working with older firmware.

## 0.8.0 (10)

A tidier page, and a tidier codebase.

- The page has six tabs: Care, Play, World (shop, outings, family, friends, wardrobe), Tune, Photo and Settings
  (the clock and alarm live there now). It remembers the tab you were on.
- When the dongle stops answering, the page says so after ten seconds and offers "Try again" instead of showing
  stale hearts.
- NEW badges disappear by themselves once you've used a newer version for a while.
- Toasts stay as long as they take to read; buttons and chips are easier to tap; Japanese labels are marked as
  Japanese for screen readers; a Share button for photos on phones that have one.
- Tapping the page no longer pauses your music: only the chirps switch the phone to playback.
- The page redraws a card only when its data changed, and follows games over the live connection instead of
  polling four times a second. Out-of-order updates are ignored.
- The page's script is named by hash in the security policy: nothing else can ever run on it.
- Developers: one `assets/tables.json` for every shared name, label and price (firmware, page, relay);
  `ui.cpp` split into themed files, pixel-identical; the clock-sanity epoch named once.

## 0.7.1 (9)

Reliability, nothing new to play with.

- An update is confirmed only once the dongle is on Wi-Fi with a real clock, never just by staying up: an update
  that broke Wi-Fi now rolls back instead of locking itself in.
- The live page and the bird's wandering keep working after 49.7 days of uptime (timer wrap).
- Care from the page or Telegram, whistles and gifts no longer cut into a game, a focus session, or a once-only
  moment such as hatching, a party or the golden reveal. The care still counts.
- A saved bird is never refused on boot, whatever firmware wrote it; a rollback keeps everything.
- The daily gift and the seed limits follow the local day, like the omikuji.
- Commands from Telegram can't be applied twice after a reboot; a pairing code waits for a free screen.
- Scritches: affection grows at the same pace as joy (one scritch per ten minutes counts).
- The setup Wi-Fi password has ten digits instead of eight.
- The dongle reports free memory, uptime and the reason for its last start (`/status`, serial `state`).
- Developers: one `VERSION` file, guarded `tools/release.py`, pinned libraries, CI, `tools/check.sh`.

## 0.7.0 (8)

Okame's world.

- Seeds 🌻: earned by care (up to 10 a day), games (up to 30 a day), the omikuji, focus sessions, parties and
  outings. Matthieu can send up to 20 a day with `/seeds`.
- The bird shop on the page: a honey stick, five cage toys (swing, bell, mirror, ladder, perch) and the
  accessories not yet unlocked.
- Cage toys: the chosen toy stands in the cage; the bird plays with it when bored, or on request.
- Outings: the park, a café, the beach, a shrine, a windmill and a frietkot. An hour or so in the daytime; nothing
  needs doing while the bird is out; it comes home with a souvenir and sends a postcard to Telegram.
- Family: from 20 days old (or as a wise old bird) a bird can find a partner; the next morning it leaves an egg,
  moves into the family album and still drops by. The chick may take after its parent.

## 0.6.0 (7)

Classic Tamagotchi features.

- Training and tricks: now and then the bird fusses for no reason; a calm "no" on the page teaches it manners and
  unlocks a trick (wave, spin, bow, hang upside down); `/trick` shows one.
- Age parties at 7, 10, 30, 50, 100, 200 and 365 days.
- A wise old bird from 30 days, with a little walking stick; a bird cared for almost perfectly turns golden.
- Left or Right?, the game from the very first Tamagotchi.
- A clock on the screen, optionally on the covered cage, and a wake-up alarm that also reaches Telegram.

## 0.5.1 (6)

- `/update`: the dongle checks GitHub for a signed release on command and reports what it found.

## 0.5.0 (5)

- Bird friends who visit: a budgie, a Belgian racing pigeon (it brings the notes and hugs), a shima-enaga in
  winter, a mejiro in plum season and a lucky owl in the evening. A friends card on the page.
- A budgie or an owl can hatch from the next egg.
- The home-screen icon for "Add to Home Screen" on iPhone.
- 0.4, shipped in the same release: Janken, Copy the dance, Feather Catch (in the bot), the daily omikuji,
  a countdown to coming home, the 💛 button, taught words, seasons and holidays, the weather outside, and the
  focus buddy.

## 0.2.0 (2)

- The first signed OTA release: the bird, the phone page, Wi-Fi setup, the Telegram relay link.

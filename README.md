# Okame

A pocket cockatiel for an ESP32-S3 USB dongle with a 0.96" screen, in the style of a 90s LCD virtual pet.
It sleeps until the dongle joins a saved Wi-Fi network, then wakes up and wants seed, baths, scritches and duets.

- `lib/okame_core/`: pet rules, mini-games, dot-matrix UI (shared by the firmware and the desktop simulator)
- `src/`: firmware (display, button, Wi-Fi + setup hotspot, phone page API, signed OTA updates)
- `web/`: the phone page served by the dongle
- `assets/sprites/`: ASCII pixel art, compiled by `tools/gen_sprites.py`
- `case/`: parametric CadQuery model of the desk egg
- `render/`: Blender Cycles product renders

Build: `pio run -e okame -t upload` · tests: `pio test -e native` · screenshots: `pio run -e sim && .pio/build/sim/program`

Updates are signed (ECDSA P-256); the dongle only installs images signed with the release key.

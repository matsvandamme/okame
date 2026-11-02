#!/bin/sh
# Everything CI runs, in one go: native tests, the page/firmware route check, generated files in sync,
# the relay tests, the simulator and the firmware build. Run it before a commit or a release.
#   tools/check.sh          everything
#   tools/check.sh quick    skip the firmware build (the slow part)
set -e
cd "$(dirname "$0")/.."

echo "== native tests"
pio test -e native | tail -1
echo "== page routes"
python3 tools/check_routes.py
echo "== generated sprites in sync"
python3 tools/gen_sprites.py >/dev/null
git diff --exit-code --stat lib/okame_core/src/sprites.h
echo "== relay tests"
uv run --with aiohttp --with anthropic --with pillow python relay/test_relay.py | tail -1
echo "== simulator"
pio run -e sim | tail -1
if [ "$1" != "quick" ]; then
  echo "== firmware"
  pio run -e okame | grep -E "Flash:|SUCCESS|FAILED"
fi
echo "all good"

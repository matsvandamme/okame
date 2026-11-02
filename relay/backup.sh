#!/bin/sh
# Nightly copy of the relay's database (users, queue, events, settings) with a week of history.
# crontab -e (as root or the okame user):  15 4 * * * /opt/okame/app/backup.sh
set -e
HOME_DIR=${OKAME_HOME:-/opt/okame}
mkdir -p "$HOME_DIR/backup"
"$HOME_DIR/venv/bin/python" - "$HOME_DIR" <<'PY'
import sqlite3, sys, datetime
home = sys.argv[1]
src = sqlite3.connect(f"{home}/okame.db")
dst = sqlite3.connect(f"{home}/backup/okame-{datetime.date.today():%a}.db")  # Mon..Sun: seven rolling copies
src.backup(dst)
dst.close(); src.close()
PY

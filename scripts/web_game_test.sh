#!/usr/bin/env bash
# Checks the exported web game starts in a browser: serves the export over
# HTTP, opens it in headless Chrome, and expects the season loaded through
# the extension ("A season of N events, M pullers") and no error in the
# console. Leaves the console in <web export>/../web_console.log and a
# screenshot in <web export>/../web_game.png.
#
#   scripts/web_game_test.sh <web export directory> [chrome] [seconds]
#
# The export is scripts/godot_export.sh's build/export/web. Needs Python 3
# (the server), Node 22 or later, and Chrome or Chromium (default
# google-chrome); no GPU: Chrome draws WebGL in software (SwiftShader).
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "usage: $0 <web export directory> [chrome] [seconds]" >&2
    exit 2
fi
web="$(realpath "$1")"
readonly web
readonly chrome="${2:-google-chrome}"
# Long enough for the 50 MB debug engine to load and compile in software,
# and the menu to start the season.
readonly seconds="${3:-30}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly root
readonly log="${web}/../web_console.log"
readonly screenshot="${web}/../web_game.png"

if [[ ! -f "${web}/index.html" ]]; then
    echo "no index.html in ${web}: export the Web preset first" >&2
    exit 1
fi

# A free port from the OS, then Python's server on it.
port="$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')"
readonly port
python3 -m http.server "${port}" --bind 127.0.0.1 --directory "${web}" >/dev/null 2>&1 &
readonly server=$!
trap 'kill "${server}"' EXIT
for _ in $(seq 50); do
    curl -fs -o /dev/null "http://127.0.0.1:${port}/index.html" && break
    sleep 0.1
done

timeout $((seconds + 60)) node "${root}/scripts/web_console.mjs" "${chrome}" \
    "http://127.0.0.1:${port}/index.html" "${seconds}" "${screenshot}" | tee "${log}"

if ! grep -qE '^log: A season of [0-9]+ events, [0-9]+ pullers' "${log}"; then
    echo "the web game did not start its season" >&2
    exit 1
fi
if grep -qE '^(error|exception):|ERROR' "${log}"; then
    echo "the web game hit an error" >&2
    exit 1
fi
echo "the web game started its season without errors"

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
BIN="${1:-$ROOT/build/plugin/TONE3000_artefacts/Release/Standalone/TONE3000}"
command -v Xvfb >/dev/null
command -v xwininfo >/dev/null
test -x "$BIN"

scratch="$(mktemp -d /tmp/t3k-artemis-window.XXXXXX)"
xvfb_pid=''
app_pid=''
cleanup() {
  if [ -n "$app_pid" ]; then kill "$app_pid" 2>/dev/null || true; fi
  if [ -n "$xvfb_pid" ]; then kill "$xvfb_pid" 2>/dev/null || true; fi
}
trap cleanup EXIT

Xvfb -displayfd 3 -screen 0 1560x720x24 3> "$scratch/display" > "$scratch/xvfb.log" 2>&1 &
xvfb_pid=$!
for _ in {1..50}; do
  if [ -s "$scratch/display" ]; then break; fi
  sleep 0.1
done
test -s "$scratch/display"
export DISPLAY=":$(cat "$scratch/display")"
XDG_CONFIG_HOME="$scratch/config" JACK_NO_START_SERVER=1 "$BIN" > "$scratch/app.log" 2>&1 &
app_pid=$!

for _ in {1..80}; do
  if xwininfo -root -tree > "$scratch/windows" 2>&1 &&
     grep -Eq '"TONE3000".*1560x720\+0\+0' "$scratch/windows"; then
    kill -0 "$app_pid"
    echo 'TONE3000 kiosk window fills 1560x720 and stays alive'
    exit 0
  fi
  if ! kill -0 "$app_pid" 2>/dev/null; then
    cat "$scratch/app.log" >&2
    exit 1
  fi
  sleep 0.1
done
cat "$scratch/windows" >&2
cat "$scratch/app.log" >&2
exit 1

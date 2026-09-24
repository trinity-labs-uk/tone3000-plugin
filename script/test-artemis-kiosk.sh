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
export GDK_BACKEND=x11
export WAYLAND_DISPLAY=
XDG_CONFIG_HOME="$scratch/config" JACK_NO_START_SERVER=1 "$BIN" > "$scratch/app.log" 2>&1 &
app_pid=$!

window_ready=0
for _ in {1..80}; do
  if xwininfo -root -tree > "$scratch/windows" 2>&1 &&
     grep -Eq '"TONE3000".*15(4[0-9]|5[0-9]|60)x7(0[0-9]|1[0-9]|20)\+0\+0' "$scratch/windows"; then
    kill -0 "$app_pid"
    window_ready=1
    break
  fi
  if ! kill -0 "$app_pid" 2>/dev/null; then
    cat "$scratch/app.log" >&2
    exit 1
  fi
  sleep 0.1
done
if [ "$window_ready" -ne 1 ]; then
  cat "$scratch/windows" >&2
  cat "$scratch/app.log" >&2
  exit 1
fi

# Wait for WebKit to paint the embedded React UI, then activate the fixed
# upper-left button through XTEST. The process must exit through JUCE.
sleep 3
python3 - <<'PY'
import ctypes
x11 = ctypes.CDLL('libX11.so.6')
xtst = ctypes.CDLL('libXtst.so.6')
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
display = x11.XOpenDisplay(None)
assert display, 'cannot open the Xvfb display'
xtst.XTestFakeMotionEvent(display, -1, 65, 35, 0)
xtst.XTestFakeButtonEvent(display, 1, 1, 0)
xtst.XTestFakeButtonEvent(display, 1, 0, 0)
x11.XFlush(display)
x11.XCloseDisplay(display)
PY
for _ in {1..50}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then
    echo 'TONE3000 fills the Artemis display and its Launchpad button exits cleanly'
    exit 0
  fi
  sleep 0.1
done
echo 'TONE3000 did not exit after pressing Back to Launchpad' >&2
tail -n 30 "$scratch/app.log" >&2
exit 1

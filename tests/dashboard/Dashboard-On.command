#!/bin/bash
# [Claude 2026-10-31] Starts the live test dashboard in the background on this Mac's tailnet address and opens it.
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"; PIDFILE="$HERE/.server.pid"; PORT=4120
ADDR="$( (tailscale ip -4 || /Applications/Tailscale.app/Contents/MacOS/Tailscale ip -4) 2>/dev/null | head -1)"; ADDR="${ADDR:-127.0.0.1}"
if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
  echo "The dashboard is already running: http://$ADDR:$PORT/"
else
  cd "$HERE/../.." && nohup "${WLA_TEST_PYTHON:-python3}" "$HERE/server.py" --host "$ADDR" --port $PORT >/dev/null 2>&1 &
  echo $! > "$PIDFILE"; sleep 2
  kill -0 "$(cat "$PIDFILE")" 2>/dev/null || { rm -f "$PIDFILE"; echo "The dashboard did not start (port $PORT may be in use)."; exit 1; }
  echo "The dashboard is running: http://$ADDR:$PORT/"
fi
open "http://$ADDR:$PORT/"

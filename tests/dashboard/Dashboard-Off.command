#!/bin/bash
# [Claude 2026-10-31] Stops the live test dashboard (pid file, else whatever dashboard listens on port 4120).
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"; PIDFILE="$HERE/.server.pid"; PORT=4120
if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
  kill "$(cat "$PIDFILE")"; echo "Stopped the dashboard (process $(cat "$PIDFILE"), from its pid file)."
else
  PID="$(lsof -ti tcp:$PORT -sTCP:LISTEN 2>/dev/null | head -1)"
  if [ -n "$PID" ] && ps -o command= -p "$PID" | grep -q server.py; then kill "$PID"; echo "Stopped the dashboard listening on port $PORT (process $PID); there was no pid file."
  elif [ -n "$PID" ]; then echo "Port $PORT is used by another program; left it alone."
  else echo "The dashboard was not running."; fi
fi
rm -f "$PIDFILE"

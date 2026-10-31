#!/bin/bash
# [Codex 2026-10-30] SSH needs the existing desktop bootstrap for AU and GUI tests.
# No services, sharing/privacy settings, host installations or first launches are configured here.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
PYTHON="${WLA_TEST_PYTHON:-python3}"
if [ "$(uname -s)" = Darwin ] && [ "$(launchctl managername)" != Aqua ]; then
  if launchctl print "gui/$(id -u)" >/dev/null 2>&1; then
    exec sudo -n launchctl asuser "$(id -u)" sudo -n -u "$(id -un)" \
      env "PATH=$PATH" "WLA_NO_LOGIC=${WLA_NO_LOGIC:-0}" \
      "WLA_TEST_BUILD_DIR=${WLA_TEST_BUILD_DIR:-}" "WLA_PLUGINVAL=${WLA_PLUGINVAL:-}" \
      "$PYTHON" "$HERE/../lib/run.py" "$@"
  fi
  echo "No logged-in desktop session; GUI checks will report SKIP."
  export WLA_NO_DESKTOP=1
fi
exec "$PYTHON" "$HERE/../lib/run.py" "$@"

#!/bin/bash
# [Codex 2026-10-30] Keep file/build/TCC operations in the invoking session.
# UI alone is delegated to an already-authorized Terminal when SSH lacks AX access.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
exec /usr/bin/python3 "$HERE/run-host.py" "$@"

#!/bin/bash
# [Codex 2026-10-30] M-07 / V-08, standalone microphone consent regression.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
[ -f "$HERE/run-terminal.sh" ] || HERE="$HERE/logic"
exec bash "$HERE/run-terminal.sh" microphone "$@"

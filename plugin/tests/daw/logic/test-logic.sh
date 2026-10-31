#!/bin/bash
# [Codex 2026-10-30] M-09; also works after moving this folder to tests/mac/logic.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
[ -f "$HERE/run-terminal.sh" ] || HERE="$HERE/logic"
exec bash "$HERE/run-terminal.sh" logic "$@"

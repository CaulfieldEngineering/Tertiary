#!/bin/bash
# [Codex 2026-10-30] W-15: independently runnable; common result contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/_run.sh" W-15 "$@"

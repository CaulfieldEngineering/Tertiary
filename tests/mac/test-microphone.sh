#!/bin/bash
# [Codex 2026-10-30] M-07: independently runnable; common result contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/_run.sh" M-07 "$@"

#!/bin/bash
# [Codex 2026-10-30] T2-03: independently runnable; common result contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/_run.sh" T2-03 "$@"

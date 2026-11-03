#!/bin/bash
# [Codex 2026-10-30] P-01: independently runnable; common result contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/../mac/_run.sh" P-01 "$@"

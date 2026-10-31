#!/bin/bash
# [Codex 2026-10-30] GH-01: independently runnable; common result contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/../mac/_run.sh" GH-01 "$@"

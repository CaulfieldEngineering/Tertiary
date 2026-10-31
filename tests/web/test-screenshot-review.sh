#!/bin/bash
# [Codex 2026-10-30] WEB-04: independently runnable; common result contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/../mac/_run.sh" WEB-04 "$@"

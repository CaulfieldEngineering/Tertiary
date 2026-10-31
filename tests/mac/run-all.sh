#!/bin/bash
# [Codex 2026-10-30] Independent release-check entry point; shared JSON contract.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$HERE/../mac/_run.sh" all "$@"

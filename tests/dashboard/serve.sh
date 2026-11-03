#!/bin/bash
# [Claude 2026-10-31] Live test dashboard for this repo: http://127.0.0.1:4120/ (see tests/README.md).
# Pass --host <this machine's tailnet address> to watch it from another of Joe's machines; never a public address.
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
exec "${WLA_TEST_PYTHON:-python3}" "$HERE/server.py" "$@"

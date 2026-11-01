#!/bin/bash
# [Claude 2026-10-31] B-06 Web demo (bash: Mac, Linux, Git Bash): native BinaryData, then Wonderlab/Emscripten to
# WebAssembly, then the assembled page, in build/web. The one web build: test.yml (web, web-mobile) and the release
# workflow's web-build job call this script, as does build-all.sh. It runs plugin/tests/web/build.mjs.
#
#   bash scripts/web/build-web.sh            the shippable demo, plus build/web-bundle (what a deploy uploads)
#   bash scripts/web/build-web.sh --test     with the read-only test inspector the browser tests need; no bundle
#   bash scripts/web/build-web.sh --dry-run
#
# Tools: Emscripten as already activated (EMSDK set, e.g. by setup-emsdk on GitHub), else the pinned emsdk in .tools/;
# Wonderlab from WONDERLAB_DIR, else the pinned commit cloned into .tools/ with this machine's git credentials;
# ninja from PATH, else .tools/. Node and Python are prerequisites. (scripts/lib/tools.json)
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
TEST=0
for a in "$@"; do
  case "$a" in --test) TEST=1 ;; --dry-run) DRY_RUN=1 ;; *) echo "unknown option $a"; exit 64 ;; esac
done
step_begin B-06 "Web demo$([ "$TEST" = 1 ] && echo ' (test build)')"
cd "$ROOT" || fail "no repo at $ROOT"

if [ -z "${WONDERLAB_DIR:-}" ]; then
  WONDERLAB_DIR="$(tool_path wonderlab)" || fail "Wonderlab is not available: set WONDERLAB_DIR to a checkout (it is a private repo)"
fi
export WONDERLAB_DIR
if [ -z "${EMSDK:-}" ]; then
  EMSDK_DIR="$(tool_path emsdk)" || fail "Emscripten SDK could not be installed into .tools/ (or set EMSDK)"
  # shellcheck disable=SC1091
  . "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1 || fail "could not activate $EMSDK_DIR"
fi
if ! command -v ninja >/dev/null 2>&1; then
  NINJA="$(tool_path ninja)" || fail "ninja is not on PATH and could not be fetched"
  PATH="$(dirname "$NINJA"):$PATH"; export PATH
fi
mkdir -p plugin/tests/web/out
ARGS=()
[ "$TEST" = 1 ] || ARGS+=(--no-inspector)
run_logged node plugin/tests/web/build.mjs "${ARGS[@]}"; code=$?
cp "$STEP_LOG" plugin/tests/web/out/build.log 2>/dev/null   # the web test wrapper reads it, as before
[ "$code" = 0 ] || fail "web build failed (see the log)"
if [ "$TEST" = 0 ]; then
  run_logged node scripts/web/bundle.mjs || fail "collecting build/web-bundle failed"
  STEP_ARTIFACTS+=("$ROOT/build/web-bundle/build-manifest.sha256")
fi
[ "$DRY_RUN" = 1 ] && step_end SKIP "dry run: commands printed, nothing built"
ID=$(sed -n 's/.*"buildId": *"\([^"]*\)".*/\1/p' build/web/test-build.json 2>/dev/null | head -1)
step_end PASS "build/web, build id ${ID:-?}, Wonderlab $(git -C "$WONDERLAB_DIR" rev-parse --short HEAD 2>/dev/null)$([ "$TEST" = 1 ] && echo ', with test inspector' || echo ', bundle in build/web-bundle')"

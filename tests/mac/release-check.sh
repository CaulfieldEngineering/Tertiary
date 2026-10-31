#!/bin/bash
# [Claude 2026-10-31] The Mac half of tests/Run-Release-Check.ps1: build this checkout at the given commit,
# then the full Run-All (Logic and microphone included) into the given results folder.
# Usage: bash tests/mac/release-check.sh <commit> <results-folder-name>
# Machine settings (WLA_PLUGINVAL, WLA_TEST_CMAKE, PATH) and the optional hooks wla_release_pause /
# wla_release_resume come from ~/.wla-test/env.sh, which is not in any repo.
set -u
COMMIT="$1"; NAME="$2"
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"; ROOT="$(cd -- "$HERE/../.." && pwd)"
[ -f "$HOME/.wla-test/env.sh" ] && . "$HOME/.wla-test/env.sh"
cd "$ROOT" || exit 9
if [ "$(git rev-parse HEAD)" != "$COMMIT" ]; then echo "Mac checkout is $(git rev-parse HEAD), not $COMMIT"; exit 9; fi
git checkout -q -- .DS_Store 2>/dev/null   # Finder metadata only; a tracked file in this repo
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then echo "Mac checkout has uncommitted changes"; git status --short --untracked-files=no; exit 9; fi
type wla_release_pause >/dev/null 2>&1 && wla_release_pause
CMAKE="${WLA_TEST_CMAKE:-cmake}"
mkdir -p "tests/results/$NAME"
if ! "$CMAKE" --build build-tests --config Release -j 6 > "tests/results/$NAME/build.log" 2>&1; then
  echo "Mac build failed; see tests/results/$NAME/build.log"; code=8
else
  bash tests/mac/run-all.sh --results-dir "tests/results/$NAME"; code=$?
fi
type wla_release_resume >/dev/null 2>&1 && wla_release_resume
exit $code

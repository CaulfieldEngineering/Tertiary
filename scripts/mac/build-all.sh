#!/bin/bash
# [Claude 2026-10-31] The Mac build suite: every Mac step in scripts/sequence.json, in order, each reported like the
# tests (scripts/results/<run>/progress.jsonl, <ID>.json, summary.md/json). "manual" steps (the staging-preview
# deploy) never run from here.
#
#   bash scripts/mac/build-all.sh [--build-dir build] [--universal] [--demo] [--no-web] [--adhoc] [--dry-run]
#        [--results-dir scripts/results/my-run] [--keep-going]
#   --adhoc      sign the plug-ins ad hoc (exercises the signing commands without a certificate)
#   --dry-run    every step prints its commands only
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
BUILD_DIR=build; DEMO=0; NOWEB=0; ADHOC=0; UNIVERSAL=0; KEEP=0; RESULTS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift ;; --demo) DEMO=1 ;; --no-web) NOWEB=1 ;; --adhoc) ADHOC=1 ;;
    --universal) UNIVERSAL=1 ;; --dry-run) DRY_RUN=1 ;; --keep-going) KEEP=1 ;; --results-dir) RESULTS="$2"; shift ;;
    *) echo "unknown option $1"; exit 64 ;;
  esac; shift
done
STARTED=$(date +%s); STARTED_AT=$(date '+%Y-%m-%d %H:%M:%S')
[ -n "$RESULTS" ] && { case "$RESULTS" in /*) ;; *) RESULTS="$ROOT/$RESULTS" ;; esac; export WLA_BUILD_RESULTS="$RESULTS"; }
WLA_BUILD_RESULTS="$("$PYTHON" "$LIB_DIR/report.py" run-start --run build-all ${WLA_BUILD_RESULTS:+--results "$WLA_BUILD_RESULTS"})"
export WLA_BUILD_RESULTS WLA_BUILD_NESTED=1
say "build-all: results in $WLA_BUILD_RESULTS"

ROWS=$("$PYTHON" - "$ROOT/scripts/sequence.json" <<'PY'
import json, sys
for r in json.load(open(sys.argv[1], encoding='utf-8')):
    s = r['scripts'].get('mac')
    if not s or r.get('manual'):
        continue
    args = ' '.join(r.get('args', {}).get('mac', []))
    print('\t'.join([r['id'], r['name'], s, '1' if r.get('build_dir') else '0', '1' if r.get('web') else '0', args]))
PY
)
ROWS=$(printf '%s\n' "$ROWS" | tr -d '\r')   # Python on Windows (Git Bash) ends lines with CR
FAILED=0
while IFS=$'\t' read -r id name script bdir web extra; do
  [ -n "$id" ] || continue
  [ "$NOWEB" = 1 ] && [ "$web" = 1 ] && continue
  if [ "$FAILED" = 1 ] && [ "$KEEP" = 0 ]; then
    "$PYTHON" "$LIB_DIR/report.py" begin --id "$id" --title "$name" --results "$WLA_BUILD_RESULTS"
    "$PYTHON" "$LIB_DIR/report.py" end --id "$id" --status SKIP --detail "not run: an earlier step failed" --results "$WLA_BUILD_RESULTS" >/dev/null
    continue
  fi
  A=()
  [ -n "$extra" ] && read -r -a A <<< "$extra"
  [ "$bdir" = 1 ] && A+=(--build-dir "$BUILD_DIR")
  [ "$id" = B-01 ] && [ "$DEMO" = 1 ] && A+=(--demo)
  [ "$id" = B-01 ] && [ "$UNIVERSAL" = 1 ] && A+=(--universal)
  [ "$id" = B-02 ] && [ "$ADHOC" = 1 ] && A+=(--adhoc)
  [ "$DRY_RUN" = 1 ] && A+=(--dry-run)
  bash "$ROOT/scripts/$script" ${A[@]+"${A[@]}"} < /dev/null
  code=$?
  if [ "$code" = 1 ]; then FAILED=1
  elif [ "$code" != 0 ] && [ "$code" != 2 ] && [ "$code" != 3 ]; then
    FAILED=1
    "$PYTHON" "$LIB_DIR/report.py" end --id "$id" --status FAIL --detail "the script exited $code without a result" --results "$WLA_BUILD_RESULTS" >/dev/null
  fi
done <<< "$ROWS"
"$PYTHON" "$LIB_DIR/report.py" run-end --run build-all --results "$WLA_BUILD_RESULTS"; code=$?
T=$(( $(date +%s) - STARTED ))
say "build-all started $STARTED_AT, ended $(date '+%Y-%m-%d %H:%M:%S'), took $((T / 3600)) h $((T % 3600 / 60)) min $((T % 60)) s"
exit $code

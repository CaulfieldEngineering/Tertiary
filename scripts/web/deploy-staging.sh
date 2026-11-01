#!/bin/bash
# [Claude 2026-10-31] B-07 Web staging preview: uploads build/web-bundle (made by build-web.sh) to the PRIVATE preview
# CaulfieldEngineering.com/plug-ins/<repo>/<wWEB_STAGING_DIR>/ (noindex, linked from nowhere). It never touches a live
# demo: those are deployed only by the release workflow on a v* tag. Moved from workflow.yml "Deploy Web Demo Preview
# (staging only)"; the workflow runs it after its SSH setup steps, on staging pushes only.
#
#   bash scripts/web/deploy-staging.sh --dry-run     what it would upload, and where
#   bash scripts/web/deploy-staging.sh               from a desk: only when this checkout is clean and IS the current
#                                                    origin/staging commit, and the bundle was built from it (no test
#                                                    inspector). SSH host: WLA_CE_SSH (default CaulfieldEngineering.com,
#                                                    as configured in ~/.ssh/config).
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
for a in "$@"; do
  case "$a" in --dry-run) DRY_RUN=1 ;; *) echo "unknown option $a"; exit 64 ;; esac
done
step_begin B-07 "Web staging preview"
set -uo pipefail
cd "$ROOT" || fail "no repo at $ROOT"

REPOSITORY_NAME="${REPOSITORY_NAME:-$wPLUGIN_NAME}"
STAGING_DIR="${STAGING_DIR:-${wWEB_STAGING_DIR:-web-staging}}"
LIVE_DIR="${wWEB_LIVE_DIR:-web}"
if ! [[ "$STAGING_DIR" =~ ^[A-Za-z0-9][A-Za-z0-9_-]*$ ]]; then fail "staging folder name '$STAGING_DIR' must be letters, digits, - or _"; fi
[ "$STAGING_DIR" != "$LIVE_DIR" ] || fail "the staging folder is the live folder ('$LIVE_DIR'); refusing"
B="$ROOT/build/web-bundle"
[ -f "$B/build-manifest.sha256" ] && [ -f "$B/build-commit.txt" ] || fail "no build/web-bundle; run scripts/web/build-web.sh first"
SHA="${GITHUB_SHA:-$(git rev-parse HEAD)}"
[ "$(tr -d '[:space:]' < "$B/build-commit.txt")" = "$SHA" ] || fail "build/web-bundle was built from another commit; rebuild it"
if grep -q '"inspector": *true' build/web/test-build.json 2>/dev/null; then fail "build/web is a test build (inspector); build without --test"; fi
CE_SERVER="${WLA_CE_SSH:-CaulfieldEngineering.com}"
BASE="/home/caulfiel/public_html/plug-ins/${REPOSITORY_NAME}"
STG="${BASE}/${STAGING_DIR}"
INC="${STG}.incoming-${GITHUB_RUN_ID:-desk$(date +%s)}-${GITHUB_RUN_ATTEMPT:-1}"
URL="https://caulfieldengineering.com/plug-ins/${REPOSITORY_NAME}/${STAGING_DIR}/"
WL="${WONDERLAB_DIR:-}"
[ -n "$WL" ] || WL="$(tool_path wonderlab --no-install)" || fail "Wonderlab (for its htaccess) not found; set WONDERLAB_DIR"
[ -f "$WL/web/htaccess" ] || fail "$WL/web/htaccess not found"

if ! in_ci && [ "$DRY_RUN" != 1 ]; then
  [ -z "$(git status --porcelain --untracked-files=no)" ] || fail "this checkout has uncommitted changes"
fi

# The preview's .htaccess: the demo's headers, plus noindex so search engines never list it
TMPD="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
HT="$TMPD/staging-htaccess"
cat "$WL/web/htaccess" > "$HT"
printf '\n# Staging preview: never indexed\nHeader always set X-Robots-Tag "noindex, nofollow, noarchive"\n' >> "$HT"

# A slower, older run must not replace a newer preview: deploy only if staging is still this commit
HEAD_SHA=$(git -C "$ROOT" ls-remote origin refs/heads/staging | cut -f1) || HEAD_SHA=""
if [ "$HEAD_SHA" != "$SHA" ] && [ "$DRY_RUN" = 1 ]; then
  say "note: staging is ${HEAD_SHA:-unknown}, not ${SHA}; a real run would stop here and deploy nothing"
elif [ "$HEAD_SHA" != "$SHA" ]; then
  echo "::notice::staging is now ${HEAD_SHA:-unknown}, not ${SHA}; this older build does not replace the preview."
  [ -n "${GITHUB_STEP_SUMMARY:-}" ] && echo "Web demo preview not updated: staging moved on to \`${HEAD_SHA:-unknown}\`." >> "$GITHUB_STEP_SUMMARY"
  step_end SKIP "not deployed: staging is ${HEAD_SHA:-unknown}, this build is ${SHA}"
fi

say "Uploading the preview to ${CE_SERVER}:${INC}"
run_logged ssh "$CE_SERVER" "rm -rf '${INC}' && mkdir -p '${INC}'" || fail "could not create ${INC}"
run_logged scp -r "${B}/"* "${CE_SERVER}:${INC}/" || fail "upload failed"
run_logged scp "$HT" "${CE_SERVER}:${INC}/.htaccess" || fail "htaccess upload failed"
# Swap the whole folder, so no file from an older preview is left behind (it would be promoted on the tag)
run_logged ssh "$CE_SERVER" "rm -rf '${STG}.old' && if [ -d '${STG}' ]; then mv '${STG}' '${STG}.old'; fi && [ ! -e '${STG}' ] && mv '${INC}' '${STG}' && rm -rf '${STG}.old'" \
  || fail "could not swap in the new preview"
[ "$DRY_RUN" = 1 ] && step_end SKIP "dry run: would deploy commit ${SHA::8} ($(wc -l < "$B/build-manifest.sha256" | tr -d ' ') files) to ${URL}"

say "Staging preview: ${URL}"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    echo "### Web demo preview (staging)"
    echo ""
    echo "Preview: ${URL}"
    echo ""
    echo "Commit \`${SHA}\`, build id \`${SHA::8}\`. Not indexed, linked from nowhere."
    echo "The live demos were not changed; a v* tag promotes these exact files."
  } >> "$GITHUB_STEP_SUMMARY"
fi
step_end PASS "deployed commit ${SHA::8} to the private preview ${URL}"

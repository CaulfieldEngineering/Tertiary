#!/usr/bin/env bash
# test_publish_steps.sh: tests for the release pipeline's download-redirect step, with a fake Redirection API
# ==========================================================================================
# [Claude 2026-10-30] That step only runs on a real v* tag, the worst moment to find a bug in it. This takes the
# step's script out of workflow.yml (publish job, "Update Download Redirect URLs") and runs it against a fake
# WordPress Redirection API (a fake curl; no network). Run: bash .github/scripts/test_publish_steps.sh
# Needs jq and python3 with PyYAML (both on GitHub's Ubuntu runners). CI runs it in preflight.
# ==========================================================================================
set -uo pipefail

# (pwd -W: Windows-style paths under Git Bash, for a native jq; Linux has no -W and uses pwd)
HERE=$(cd "$(dirname "$0")" && { pwd -W 2>/dev/null || pwd; })
WORKDIR=$(mktemp -d)   # for PATH (a C:/ path's colon would split PATH)
WORK=$(cd "$WORKDIR" && { pwd -W 2>/dev/null || pwd; })
trap 'rm -rf "$WORKDIR"' EXIT
mkdir -p "$WORK/bin"

python3 - "$HERE/../workflows/workflow.yml" "$WORK/redirects.sh" <<'PY' || { echo "FAIL  could not read the step from workflow.yml"; exit 1; }
import sys, yaml
steps = yaml.safe_load(open(sys.argv[1], encoding="utf-8"))["jobs"]["publish"]["steps"]
run = [s["run"] for s in steps if s.get("name") == "Update Download Redirect URLs"]
open(sys.argv[2], "w", encoding="utf-8", newline="\n").write(run[0])
PY

# The fake Redirection API. State: $FAKE/state.json, a list of {id, url, action_data: {url}}.
# FAIL_SOURCE=<path>: a POST for that redirect answers HTTP 500. IGNORE=1: POSTs answer 200 and change nothing.
# IGNORE_SOURCE=<path>: the same, for that redirect only. READ_FAIL_SOURCE=<path>: a POST for it goes through,
# then the next read of it fails (HTTP 500), once. IGNORE_AFTER_FIRST=<path>: its first POST goes through, later ones
# are ignored (so putting it back fails).
cat > "$WORK/bin/curl" <<'FAKE'
#!/usr/bin/env bash
out="" body="" fail=0 wfmt="" url=""
while [ $# -gt 0 ]; do
  case "$1" in
    -o) out=$2; shift 2 ;;
    -d) body=$2; shift 2 ;;
    -w) wfmt=$2; shift 2 ;;
    -X|-u|-H) shift 2 ;;
    -fsS|-f) fail=1; shift ;;
    -*) shift ;;
    *) url=$1; shift ;;
  esac
done
st="$FAKE/state.json" code=200 resp=""
case "$url" in
  */group) resp='{"items":[{"id":1}]}' ;;
  *redirect\?search=*)
    q=${url#*search=}
    if [ "$q" = "${READ_FAIL_SOURCE:-}" ] && [ -f "$FAKE/posted" ] && [ ! -f "$FAKE/read-failed" ]; then
      touch "$FAKE/read-failed"; code=500; resp='<html>error</html>'
    else resp=$(jq -c --arg u "$q" '{items: [.[] | select(.url == $u)]}' "$st"); fi ;;
  */redirect|*/redirect/*)
    u=$(jq -r .url <<< "$body"); t=$(jq -r .action_data.url <<< "$body")
    if [ "$u" = "${FAIL_SOURCE:-}" ]; then code=500; resp='<html>error</html>'
    elif [ "${IGNORE:-0}" = 1 ] || [ "$u" = "${IGNORE_SOURCE:-}" ] || { [ "$u" = "${IGNORE_AFTER_FIRST:-}" ] && [ -f "$FAKE/first-done" ]; }; then resp='{"id":9}'
    else
      [ "$u" = "${READ_FAIL_SOURCE:-}" ] && touch "$FAKE/posted"
      [ "$u" = "${IGNORE_AFTER_FIRST:-}" ] && touch "$FAKE/first-done"
      jq --arg u "$u" --arg t "$t" 'if any(.[]; .url == $u) then map(if .url == $u then .action_data.url = $t else . end)
                                    else . + [{id: (length + 1), url: $u, action_data: {url: $t}}] end' "$st" > "$st.tmp" && mv "$st.tmp" "$st"
      resp=$(jq -c --arg u "$u" '.[] | select(.url == $u)' "$st")
    fi ;;
esac
if [ -n "$out" ]; then printf '%s' "$resp" > "$out"; else printf '%s' "$resp"; fi
[ -n "$wfmt" ] && printf '%s' "$code"
[ "$fail" = 1 ] && [ "$code" -ge 400 ] && exit 22
exit 0
FAKE
chmod +x "$WORK/bin/curl"

export PATH="$WORKDIR/bin:$PATH" FAKE="$WORK"
export WORDPRESS_SITE=https://store.invalid WORDPRESS_USERNAME=u WORDPRESS_APPLICATION_PASSWORD=p
export GITHUB_REPOSITORY=CaulfieldEngineering/Tertiary MAC_URL=https://store.invalid/new.pkg WIN_URL=https://store.invalid/new.exe

start() {   # both redirects on the old installers (ONLY_MAC=1: the Windows one doesn't exist)
  [ "${ONLY_MAC:-0}" = 1 ] && { echo '[{"id":1,"url":"/tertiary-download-mac","action_data":{"url":"https://store.invalid/old.pkg"}}]' > "$WORK/state.json"; rm -f "$WORK/posted" "$WORK/read-failed" "$WORK/first-done"; return; }
  echo '[{"id":1,"url":"/tertiary-download-mac","action_data":{"url":"https://store.invalid/old.pkg"}},
         {"id":2,"url":"/tertiary-download-windows","action_data":{"url":"https://store.invalid/old.exe"}}]' > "$WORK/state.json"
  rm -f "$WORK/posted" "$WORK/read-failed" "$WORK/first-done"
}
targets() { jq -c '[.[] | .action_data.url | sub("https://store.invalid/"; "")]' "$WORK/state.json"; }

PASS=0 FAILED=0
check() {   # check <name> <want exit> <want targets> <want text> [env ...]
  local name=$1 want=$2 wantTargets=$3 text=$4; shift 4
  start
  local out got now
  out=$(env "$@" bash "$WORK/redirects.sh" 2>&1); got=$?
  now=$(targets)
  if [ "$got" = "$want" ] && [ "$now" = "$wantTargets" ] && grep -qF -- "$text" <<< "$out"; then
    echo "  pass  $name"; PASS=$((PASS+1))
  else
    echo "  FAIL  $name (exit $got, wanted $want; redirects $now, wanted $wantTargets; wanted text: $text)"
    sed 's/^/        /' <<< "$out"; FAILED=$((FAILED+1))
  fi
}

echo "workflow.yml publish: Update Download Redirect URLs"
check "both redirects change and read back"            0 '["new.pkg","new.exe"]' "Redirect URLs updated successfully"
check "Windows fails: the Mac one is put back"         1 '["old.pkg","old.exe"]' "both redirects were put back"     FAIL_SOURCE=/tertiary-download-windows
check "Mac fails first: nothing changes"               1 '["old.pkg","old.exe"]' "(HTTP 500)"                       FAIL_SOURCE=/tertiary-download-mac
check "API says 200 but changes nothing: read-back fails" 1 '["old.pkg","old.exe"]' "reads back as"                 IGNORE=1
check "Windows says 200, changes nothing: Mac put back" 1 '["old.pkg","old.exe"]' "both redirects were put back"    IGNORE_SOURCE=/tertiary-download-windows
check "Mac goes through, its read-back fails: put back" 1 '["old.pkg","old.exe"]' "both redirects were put back"    READ_FAIL_SOURCE=/tertiary-download-mac
check "Windows goes through, read-back fails: both back" 1 '["old.pkg","old.exe"]' "both redirects were put back"   READ_FAIL_SOURCE=/tertiary-download-windows
check "Windows fails and Mac can't be put back: says so" 1 '["new.pkg","old.exe"]' "putting the redirects back failed" FAIL_SOURCE=/tertiary-download-windows IGNORE_AFTER_FIRST=/tertiary-download-mac
ONLY_MAC=1 check "a redirect missing: stops, nothing changed"     1 '["old.pkg"]'            "must both exist"

echo "$PASS passed, $FAILED failed"
[ "$FAILED" = 0 ]

#!/bin/bash
# [Claude 2026-11-03] Real Safari on the Mac, one step per call, for the hands-on checks agents run (WEB-02 and the
# staging-preview checks; first written as ~/macday/safari-111/run.sh for the Tertiary 1.1.1 live check). In the suite
# now so each step reports itself and has a limit (scripts/lib/stepwatch.py), with no agent remembering to check in.
#
#   bash tests/mac/safari-step.sh open URL | shot NAME | dump [DEPTH] | click X Y | audio | wait SECONDS | close | done
#
# Each call is a step of the job $WLA_WATCHDOG_JOB (default tertiary-mac-safari-<date>), stopped at its limit (exit 124).
# After it, the job reports "between Safari steps" for 20 min, so an agent that stalls between steps is caught too;
# `close` (quits Safari the normal way) or `done` ends the job. The HQ Step Watchdog reads ~/.wla-test/watchdog over SSH.
# Screenshots and the accessibility dump go to $WLA_SAFARI_DIR (default ~/macday/safari). $WLA_MAC_UI is the
# accessibility/click tool (default ~/macday/mac-ui). Never touches audio services; `audio` only reads pmset assertions.
set -u
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"; ROOT="$(cd -- "$HERE/../.." && pwd)"
JOB="${WLA_WATCHDOG_JOB:-tertiary-mac-safari-$(date +%Y-%m-%d)}"
if [ "$(launchctl managername)" != Aqua ]; then
  exec sudo -n launchctl asuser "$(id -u)" sudo -n -u "$(id -un)" env "PATH=$PATH" "HOME=$HOME" \
    "WLA_WATCHDOG_JOB=$JOB" "WLA_WATCHDOG=${WLA_WATCHDOG:-}" "WLA_WATCHDOG_DIR=${WLA_WATCHDOG_DIR:-$HOME/.wla-test/watchdog}" \
    "WLA_SAFARI_DIR=${WLA_SAFARI_DIR:-}" "WLA_MAC_UI=${WLA_MAC_UI:-}" "WLA_TEST_PYTHON=${WLA_TEST_PYTHON:-}" \
    /bin/bash "$0" "$@"
fi
export WLA_WATCHDOG_JOB="$JOB"
PYTHON="${WLA_TEST_PYTHON:-python3}"
W="$ROOT/scripts/lib/stepwatch.py"
D="${WLA_SAFARI_DIR:-$HOME/macday/safari}"; UI="${WLA_MAC_UI:-$HOME/macday/mac-ui}"
mkdir -p "$D"

# step MINUTES "what" COMMAND...: the command under its limit, then "between steps" for the agent's turn
step() {
  local minutes="$1" what="$2"; shift 2
  "$PYTHON" "$W" run --step "Safari: $what" --minutes "$minutes" -- "$@"
  local code=$?
  "$PYTHON" "$W" beat --step "between Safari steps (last: $what, exit $code)" --minutes 10
  return "$code"
}

case "${1:-}" in
  open)  step 2 "open ${2:?URL}" /bin/bash -c 'open -a Safari "$1" && sleep 2 && "$2" activate Safari' _ "$2" "$UI" ;;
  shot)  step 1 "screenshot ${2:?NAME}" screencapture -x "$D/$2.png" && echo "$D/$2.png" ;;
  dump)  step 2 "accessibility dump" /bin/bash -c '"$1" dump Safari "$2" > "$3" && wc -c "$3"' _ "$UI" "${2:-40}" "$D/ax.json" ;;
  click) step 1 "click ${2:?X} ${3:?Y}" /bin/bash -c '"$1" activate Safari && sleep 0.5 && "$1" cgclick "$2" "$3"' _ "$UI" "$2" "$3" ;;
  audio) step 1 "audio assertions" /bin/bash -c 'pmset -g assertions | grep -i -E "audio|coreaudio"' ;;
  wait)  s="${2:?SECONDS}"; step $(( s / 60 + 2 )) "listening ${s} s" sleep "$s" ;;
  close) step 1 "quit Safari" osascript -e 'tell application "Safari" to quit'; code=$?; "$PYTHON" "$W" done --note "closed"; exit $code ;;
  done)  "$PYTHON" "$W" done --note "done" ;;
  *)     echo "usage: $0 open URL | shot NAME | dump [DEPTH] | click X Y | audio | wait SECONDS | close | done"; exit 64 ;;
esac

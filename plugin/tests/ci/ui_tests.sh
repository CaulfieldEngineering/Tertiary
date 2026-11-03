#!/bin/bash
# ui_tests.sh
# Written by Claude (2026-10-29). Runs a UI test program (built on tests/ui/UiRobot.h) in CI and makes its result
# readable: the full log in <results>/ui-tests-<name>.log, the pictures in <results>/ui-snapshots-<name>, and every
# failing check (its scenario, test-plan IDs and why) on the run's summary page. Generic: nothing plugin-specific.
#
#   bash ui_tests.sh <UiTest executable> <results folder> <name for this machine> [more options for the program]
#
# The program stops a scenario that hangs (and prints where it was); this also stops the whole run after 20 minutes.

set -uo pipefail
EXE="$1"; RESULTS="$2"; NAME="$3"; shift 3
mkdir -p "$RESULTS"
LOG="$RESULTS/ui-tests-$NAME.log"

# a time limit for the whole run: timeout (Linux, Git Bash), gtimeout (Homebrew), or perl's alarm (macOS has no timeout)
limit() {
    if command -v timeout >/dev/null 2>&1; then timeout "$@"
    elif command -v gtimeout >/dev/null 2>&1; then gtimeout "$@"
    else perl -e '$t = shift; $SIG{ALRM} = sub { kill "KILL", $pid; exit 124 }; $pid = fork; if ($pid == 0) { exec @ARGV } alarm $t; waitpid $pid, 0; exit ($? >> 8)' "$@"
    fi
}

status=0
limit 1200 "$EXE" --snapshots "$RESULTS/ui-snapshots-$NAME" "$@" > "$LOG" 2>&1 || status=$?
cat "$LOG"
[ "$status" -eq 124 ] && echo "FAIL the UI tests were still running after 20 minutes and were stopped" | tee -a "$LOG"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "### UI tests ($NAME)"
        if grep -qE '^=== UI tests' "$LOG"; then
            sed -n '/^=== UI tests/,$p' "$LOG" | head -80 | sed 's/^/    /'
        else
            echo "The program stopped early (exit code $status). Its last lines:"
            tail -50 "$LOG" | sed 's/^/    /'
        fi
    } >> "$GITHUB_STEP_SUMMARY"
fi
exit "$status"

#!/bin/bash
# installer_test_mac.sh
# Written by Claude (2026-10-27). Joe: "I don't have a real Mac (anymore)... Try to automate as much of the testing
# as possible." Tests the SIGNED, NOTARIZED Mac installer that the staging pipeline built, on a real Mac
# (GitHub's), the way a customer would get it. Used by .github/workflows/installer-test.yml.
#
#   bash installer_test_mac.sh <installer .pkg> <HostTest executable> <pluginval executable>
#
# Every step runs even if an earlier one failed, so one run shows everything. A summary table goes to
# results/installer-summary.md (and the job summary). Exit code 1 if any step failed.

set -uo pipefail

PKG="$1"
HOSTTEST="$2"
PLUGINVAL="$3"
mkdir -p results
SUMMARY=results/installer-summary.md
FAILED=0

COMPONENT=/Library/Audio/Plug-Ins/Components/Tertiary.component
VST3=/Library/Audio/Plug-Ins/VST3/Tertiary.vst3
APP=/Applications/Tertiary/Tertiary.app

echo "| Step | Result | Detail |" > "$SUMMARY"
echo "|---|---|---|" >> "$SUMMARY"

# In the self-test (UNSIGNED=1: installers built unsigned on a claude/installer-test branch) the signature,
# notarization and Gatekeeper steps can't pass, so they are reported as SKIP.
signed_step() {
    if [ "${UNSIGNED:-0}" = "1" ]; then
        echo "SKIP  $1 (unsigned self-test)"
        echo "| $1 | SKIP | unsigned self-test: only the staging pipeline signs |" >> "$SUMMARY"
    else
        step "$@"
    fi
}

# step "<name>" <command...>: runs it, logs the output, records PASS or FAIL
step() {
    local name="$1"; shift
    local log="results/$(echo "$name" | tr ' /:' '___').log"
    if "$@" > "$log" 2>&1; then
        echo "PASS  $name"
        echo "| $name | PASS | $(tail -n 1 "$log" | tr '|' '/' | cut -c1-120) |" >> "$SUMMARY"
    else
        echo "FAIL  $name   (see $log)"
        tail -n 20 "$log"
        echo "| $name | **FAIL** | $(tail -n 1 "$log" | tr '|' '/' | cut -c1-120) |" >> "$SUMMARY"
        FAILED=1
    fi
}

echo "=== $(sw_vers -productName) $(sw_vers -productVersion), $(uname -m), installer: $(basename "$PKG")"

# 1. The installer as downloaded: signed, notarized, ticket stapled, accepted by Gatekeeper
signed_step "installer signature"      pkgutil --check-signature "$PKG"
signed_step "installer Gatekeeper"     spctl --assess --type install -vv "$PKG"
signed_step "installer notarization"   xcrun stapler validate "$PKG"

# 2. Install it, as a customer does (all components)
step "install"                  sudo installer -pkg "$PKG" -target /

# 3. Files where Logic and other DAWs look
step "AU in Components"         test -d "$COMPONENT"
step "VST3 in VST3 folder"      test -d "$VST3"
step "standalone in Applications" test -d "$APP"

# 4. Each installed bundle: signature intact, accepted by Gatekeeper
for bundle in "$COMPONENT" "$VST3" "$APP"; do
    name=$(basename "$bundle")
    signed_step "codesign $name" codesign --verify --deep --strict --verbose=2 "$bundle"
done
signed_step "Gatekeeper Tertiary.app"  spctl --assess --type execute -vv "$APP"
for bundle in "$COMPONENT" "$VST3"; do
    # Plug-in bundles aren't apps: Gatekeeper checks them as signed, notarized code ("open" context)
    signed_step "Gatekeeper $(basename "$bundle")" spctl --assess --type open --context context:primary-signature -vv "$bundle"
done

# 5. Apple's AU validator on the installed AU
killall -9 AudioComponentRegistrar 2>/dev/null || true
step "auval installed AU"       bash -c 'auval -strict -v aufx EXPL WLAD | tee /dev/stderr | grep -q "AU VALIDATION SUCCEEDED"'

# 6. pluginval and the host test on the installed plugins (an unlicensed copy: the demo gaps must be there)
step "pluginval installed VST3" "$PLUGINVAL" --strictness-level 10 --validate-in-process --validate "$VST3"
step "pluginval installed AU"   "$PLUGINVAL" --strictness-level 10 --validate-in-process --validate "$COMPONENT"
step "HostTest installed VST3"  "$HOSTTEST" --vst3 "$VST3" --expect-demo
step "HostTest installed AU"    "$HOSTTEST" --au --expect-demo

# 7. The standalone app starts, runs for a few seconds and quits cleanly when asked
standalone_check() {
    "$APP/Contents/MacOS/Tertiary" &
    local pid=$!
    sleep 6
    if ! kill -0 "$pid" 2>/dev/null; then echo "the app exited by itself within 6 s"; return 1; fi
    osascript -e 'tell application "Tertiary" to quit' || kill -TERM "$pid"
    for _ in $(seq 1 20); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
    if kill -0 "$pid" 2>/dev/null; then echo "still running 10 s after quit"; kill -9 "$pid"; return 1; fi
    wait "$pid"; local code=$?
    if ls ~/Library/Logs/DiagnosticReports/Tertiary* >/dev/null 2>&1; then echo "crash report written"; return 1; fi
    echo "started, ran 6 s, quit with exit code $code"
    [ "$code" -eq 0 ] || [ "$code" -eq 143 ]   # 143 = quit by SIGTERM, if the quit message wasn't handled
}
step "standalone starts and quits" standalone_check

# 8. Uninstall (macOS has no uninstaller for .pkg installs: remove the files and the receipts) and confirm it's clean
uninstall() {
    sudo rm -rf "$COMPONENT" "$VST3" /Applications/Tertiary
    for id in $(pkgutil --pkgs | grep -i tertiary); do sudo pkgutil --forget "$id"; done
    local left=""
    for f in "$COMPONENT" "$VST3" /Applications/Tertiary; do [ -e "$f" ] && left="$left $f"; done
    [ -n "$(pkgutil --pkgs | grep -i tertiary)" ] && left="$left receipts:$(pkgutil --pkgs | grep -i tertiary | tr '\n' ' ')"
    if [ -n "$left" ]; then echo "left behind:$left"; return 1; fi
    echo "files and receipts removed"
}
step "uninstall and confirm clean" uninstall

cp ~/Library/Logs/DiagnosticReports/*ertiary* ~/Library/Logs/DiagnosticReports/*luginval* results/ 2>/dev/null || true
cat "$SUMMARY"
exit $FAILED

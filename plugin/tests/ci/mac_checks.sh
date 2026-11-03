#!/bin/bash
# mac_checks.sh
# Written by Claude (2026-10-27) for Tertiary 1.1. Used by .github/workflows/test.yml on both Macs.
#
#   bash mac_checks.sh <folder with the built files> <pluginval executable> <name for the results>
#
# 1. Both chip types (arm64, x86_64) in every binary, and the oldest macOS is 11.0
# 1b. [Claude 2026-10-30] V-08: the standalone has its microphone text, the entitlements file is valid, and the
#     standalone signed with workflow.yml's command (ad-hoc identity here) holds the audio-input entitlement
# 2. The unit tests (TertiaryTests)
# 2b. [Claude 2026-10-29] The UI tests (UiTest): the real editor off screen, driven by mouse and keyboard events. A
#     failure doesn't stop the checks below; it fails the script at the end.
# 3. Apple's AU validator (auval), on the AU installed in ~/Library/Audio/Plug-Ins/Components (test plan T1-13).
#    [Claude 2026-10-29] Its codes are read from config.cmake, and a failure says plainly what failed.
# 4. pluginval on the VST3 and the AU
# 4b. The host test (HostTest) on the VST3 and the AU
# 5. A short offline render (ReviewRender), for the cross-platform comparison
# Logs go in ./results. Exits non-zero on the first failure.

set -euo pipefail

OUT="$1"
PLUGINVAL="$2"
NAME="$3"
STRICTNESS="${STRICTNESS:-10}"
mkdir -p results

# On any failure, keep macOS crash reports (stack traces) with the logs
trap 'cp ~/Library/Logs/DiagnosticReports/*luginval* ~/Library/Logs/DiagnosticReports/*auval* results/ 2>/dev/null || true' EXIT

echo "=== $NAME: $(sw_vers -productName) $(sw_vers -productVersion), $(uname -m)" | tee results/machine.log

# 1. Architectures and minimum macOS
# ==========================================================================================
check_binary() {
    local bin="$1"
    local archs
    archs=$(lipo -archs "$bin")
    echo "$bin: $archs" | tee -a results/architectures.log
    [[ "$archs" == *arm64* && "$archs" == *x86_64* ]] || { echo "ERROR: $bin is not universal"; exit 1; }
    for arch in arm64 x86_64; do
        local minos
        minos=$(vtool -arch "$arch" -show-build "$bin" | awk '/minos/ {print $2}')
        echo "  $arch minimum macOS: $minos" | tee -a results/architectures.log
        [[ "$minos" == "11.0" ]] || { echo "ERROR: $bin ($arch) needs macOS $minos, expected 11.0"; exit 1; }
    done
}
check_binary "$OUT/Tertiary.vst3/Contents/MacOS/Tertiary"
check_binary "$OUT/Tertiary.component/Contents/MacOS/Tertiary"
check_binary "$OUT/ReviewRender"
check_binary "$OUT/TertiaryTests"
check_binary "$OUT/HostTest"
check_binary "$OUT/UiTest"
check_binary "$OUT/Tertiary.app/Contents/MacOS/Tertiary"

# 1b. [Claude 2026-10-30] V-08, the standalone's audio input. macOS gives the standalone silence from the audio
#     interface unless its Info.plist has the microphone text (NSMicrophoneUsageDescription, from
#     MICROPHONE_PERMISSION_TEXT in plugin/CMakeLists.txt) and, signed with the hardened runtime, it holds the
#     audio-input entitlement (package_config/mac/standalone.entitlements). Real signing only runs on staging and tags
#     (workflow.yml, "Sign Mac Binaries"), so here a copy is signed with that same command and an ad-hoc identity
#     ("-", no timestamp), and the result is checked.
# ==========================================================================================
APP_ENTITLEMENTS=package_config/mac/standalone.entitlements
mic_fail() {
    local message="FAIL V-08 standalone audio input ($NAME): $1"
    echo "$message"
    [ -n "${GITHUB_STEP_SUMMARY:-}" ] && echo "$message" >> "$GITHUB_STEP_SUMMARY"
    exit 1
}
MIC_TEXT=$(/usr/libexec/PlistBuddy -c "Print :NSMicrophoneUsageDescription" "$OUT/Tertiary.app/Contents/Info.plist" 2>/dev/null) \
    || mic_fail "Tertiary.app's Info.plist has no NSMicrophoneUsageDescription (MICROPHONE_PERMISSION_ENABLED in plugin/CMakeLists.txt)"
[ -n "$MIC_TEXT" ] || mic_fail "Tertiary.app's NSMicrophoneUsageDescription is empty"
echo "Standalone microphone text: $MIC_TEXT" | tee results/standalone-mic.log
plutil -lint "$APP_ENTITLEMENTS" | tee -a results/standalone-mic.log || mic_fail "$APP_ENTITLEMENTS is not a valid plist"
[ "$(/usr/libexec/PlistBuddy -c "Print :com.apple.security.device.audio-input" "$APP_ENTITLEMENTS" 2>/dev/null)" = "true" ] \
    || mic_fail "$APP_ENTITLEMENTS does not set com.apple.security.device.audio-input to true"
# The signing command in workflow.yml must still pass the entitlements file to the standalone (and only to it)
SIGN_STEP=$(awk '/- name: Sign Mac Binaries/{on=1} on&&/- name: Create Standalone Package/{exit} on' .github/workflows/workflow.yml)
[ "$(grep -c -- '--entitlements "\$APP_ENTITLEMENTS"' <<< "$SIGN_STEP")" = "1" ] \
    || mic_fail "workflow.yml's Sign Mac Binaries step should pass --entitlements \"\$APP_ENTITLEMENTS\" exactly once (to the standalone)"
grep -q 'APP_ENTITLEMENTS="${GITHUB_WORKSPACE}/'"$APP_ENTITLEMENTS"'"' <<< "$SIGN_STEP" \
    || mic_fail "workflow.yml's Sign Mac Binaries step doesn't point APP_ENTITLEMENTS at $APP_ENTITLEMENTS"
SIGN_DIR=$(mktemp -d)
cp -R "$OUT/Tertiary.app" "$SIGN_DIR/"
codesign --force --sign - --verbose --deep --strict --options=runtime --timestamp=none \
    --entitlements "$APP_ENTITLEMENTS" "$SIGN_DIR/Tertiary.app" 2>&1 | tee -a results/standalone-mic.log
codesign --verify --deep --strict --verbose=2 "$SIGN_DIR/Tertiary.app" 2>&1 | tee -a results/standalone-mic.log
SIGN_INFO=$(codesign -dv --verbose=2 "$SIGN_DIR/Tertiary.app" 2>&1)
grep -q "runtime" <<< "$SIGN_INFO" || mic_fail "the test signature has no hardened runtime"
SIGN_ENT=$(codesign --display --entitlements - --xml "$SIGN_DIR/Tertiary.app" 2>/dev/null)
echo "$SIGN_ENT" >> results/standalone-mic.log
grep -q "com.apple.security.device.audio-input" <<< "$SIGN_ENT" || mic_fail "the test signature has no audio-input entitlement"
rm -rf "$SIGN_DIR"
echo "PASS V-08 standalone microphone text, entitlements file and signing command ($NAME)"

# 2. Unit tests
# ==========================================================================================
"$OUT/TertiaryTests" 2>&1 | tee results/unit-tests.log

# 2b. [Claude 2026-10-29] UI tests; failing checks (scenario, test-plan IDs, why) also go on the run's summary page
# ==========================================================================================
ui_status=0
bash plugin/tests/ci/ui_tests.sh "$OUT/UiTest" results "$NAME" || ui_status=$?

# 3. auval (T1-13). The AU codes come from config.cmake, as JUCE gets them: aufx (an effect; aumu for a synth),
#    wPLUGIN_CODE, wPLUGIN_MANUFACTURER_CODE. [Claude 2026-10-29] Were written out here (aufx EXPL WLAD).
# ==========================================================================================
cfg() { sed -n "s/^set($1[[:space:]]*\"\([^\"]*\)\").*/\1/p" config.cmake | head -1; }
AU_SUBTYPE=$(cfg wPLUGIN_CODE)
AU_MANUFACTURER=$(cfg wPLUGIN_MANUFACTURER_CODE)
AU_TYPE=aufx
[ "$(sed -n 's/^set(wIS_SYNTH[[:space:]]*\([A-Z]*\)).*/\1/p' config.cmake)" = "TRUE" ] && AU_TYPE=aumu
echo "auval codes from config.cmake: $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER" | tee results/auval-codes.log
if [ -z "$AU_SUBTYPE" ] || [ -z "$AU_MANUFACTURER" ]; then
    echo "FAIL T1-13 auval ($NAME): couldn't read wPLUGIN_CODE and wPLUGIN_MANUFACTURER_CODE from config.cmake"
    exit 1
fi
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/Components/Tertiary.component ~/Library/Audio/Plug-Ins/VST3/Tertiary.vst3
cp -R "$OUT/Tertiary.component" ~/Library/Audio/Plug-Ins/Components/
cp -R "$OUT/Tertiary.vst3" ~/Library/Audio/Plug-Ins/VST3/
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -a 2>&1 | grep -i "$AU_MANUFACTURER" | tee results/auval-list.log || true
auval -strict -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER" > results/auval.log 2>&1 || true
cat results/auval.log
if grep -q "AU VALIDATION SUCCEEDED" results/auval.log; then
    echo "PASS T1-13 auval -strict -v $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER ($NAME)"
else
    reason=$(grep -iE "FAIL|ERROR|not found|cannot|didn't" results/auval.log | head -5 | tr '\n' ' ')
    message="FAIL T1-13 auval -strict -v $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER ($NAME): the AU did not pass Apple's validation. ${reason:-The whole output is in results/auval.log.}"
    echo "$message"
    [ -n "${GITHUB_STEP_SUMMARY:-}" ] && echo "$message" >> "$GITHUB_STEP_SUMMARY"
    exit 1
fi

# 4. pluginval, VST3 and AU
# ==========================================================================================
"$PLUGINVAL" --strictness-level "$STRICTNESS" --validate-in-process --output-dir results \
    --validate ~/Library/Audio/Plug-Ins/VST3/Tertiary.vst3 2>&1 | tee results/pluginval-vst3.log
"$PLUGINVAL" --strictness-level "$STRICTNESS" --validate-in-process --output-dir results \
    --validate ~/Library/Audio/Plug-Ins/Components/Tertiary.component 2>&1 | tee results/pluginval-au.log

# 4b. The host test: loads the built VST3 and the installed AU as a DAW does (unlicensed runner: demo gaps expected)
# ==========================================================================================
"$OUT/HostTest" --vst3 ~/Library/Audio/Plug-Ins/VST3/Tertiary.vst3 --expect-demo 2>&1 | tee results/hosttest-vst3.log
"$OUT/HostTest" --au --expect-demo 2>&1 | tee results/hosttest-au.log

# 5. Render
# ==========================================================================================
python3 plugin/tests/regression/render_matrix.py "$OUT/ReviewRender" plugin/sample_audio results/renders --quick 2>&1 | tee results/renders.log

if [ "$ui_status" -ne 0 ]; then
    echo "FAIL $NAME: the UI tests failed (their failing checks are above, and on the summary page); the other checks passed"
    exit "$ui_status"
fi
echo "=== $NAME: all checks passed"

#!/bin/bash
# mac_checks.sh
# Written by Claude (2026-10-27) for Tertiary 1.1. Used by .github/workflows/test.yml on both Macs.
#
#   bash mac_checks.sh <folder with the built files> <pluginval executable> <name for the results>
#
# 1. Both chip types (arm64, x86_64) in every binary, and the oldest macOS is 11.0
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

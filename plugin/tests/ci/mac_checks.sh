#!/bin/bash
# mac_checks.sh
# Written by Claude (2026-10-27) for Tertiary 1.1. Used by .github/workflows/test.yml on both Macs.
#
#   bash mac_checks.sh <folder with the built files> <pluginval executable> <name for the results>
#
# 1. Both chip types (arm64, x86_64) in every binary, and the oldest macOS is 11.0
# 2. The unit tests (TertiaryTests)
# 3. Apple's AU validator (auval), on the AU installed in ~/Library/Audio/Plug-Ins/Components
# 4. pluginval on the VST3 and the AU
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

# 2. Unit tests
# ==========================================================================================
"$OUT/TertiaryTests" 2>&1 | tee results/unit-tests.log

# 3. auval. The AU codes come from config.cmake: aufx (effect), plugin code EXPL, manufacturer WLAD.
# ==========================================================================================
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/Components/Tertiary.component ~/Library/Audio/Plug-Ins/VST3/Tertiary.vst3
cp -R "$OUT/Tertiary.component" ~/Library/Audio/Plug-Ins/Components/
cp -R "$OUT/Tertiary.vst3" ~/Library/Audio/Plug-Ins/VST3/
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -a 2>&1 | grep -i "WLAD" | tee results/auval-list.log || true
auval -strict -v aufx EXPL WLAD 2>&1 | tee results/auval.log
grep -q "AU VALIDATION SUCCEEDED" results/auval.log || { echo "ERROR: auval failed"; exit 1; }

# 4. pluginval, VST3 and AU
# ==========================================================================================
"$PLUGINVAL" --strictness-level "$STRICTNESS" --validate-in-process --output-dir results \
    --validate ~/Library/Audio/Plug-Ins/VST3/Tertiary.vst3 2>&1 | tee results/pluginval-vst3.log
"$PLUGINVAL" --strictness-level "$STRICTNESS" --validate-in-process --output-dir results \
    --validate ~/Library/Audio/Plug-Ins/Components/Tertiary.component 2>&1 | tee results/pluginval-au.log

# 5. Render
# ==========================================================================================
python3 plugin/tests/regression/render_matrix.py "$OUT/ReviewRender" plugin/sample_audio results/renders --quick 2>&1 | tee results/renders.log

echo "=== $NAME: all checks passed"

#!/bin/bash
# [Claude 2026-10-31] B-01 Plug-in (Mac): configure and build the formats in config.cmake (VST3, AU, Standalone).
# The same script runs on the Mac mini and on GitHub (test.yml mac-build, workflow.yml build). No sudo: the build is
# not copied into ~/Library or /Library plug-in folders unless --copy-to-system is given.
#
#   bash scripts/mac/build-plugin.sh [--build-dir build] [--universal] [--tests] [--review-render] [--demo]
#        [--target Tertiary_VST3 ...] [--juce-source DIR] [--generator Ninja] [--ccache] [--config Release]
#        [--cmake-arg -DX=Y ...] [--no-configure] [--dry-run]
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
BUILD_DIR=build; CONFIG=Release; DEMO=OFF; TESTS=0; REVIEW=0; UNIVERSAL=0; JUCE=""; GEN=""; CCACHE=0; COPY=OFF; CONFIGURE=1
TARGETS=(); EXTRA=()
while [ $# -gt 0 ]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift ;;
    --config) CONFIG="$2"; shift ;;
    --demo) DEMO=ON ;;
    --tests) TESTS=1 ;;
    --review-render) REVIEW=1 ;;
    --universal) UNIVERSAL=1 ;;
    --target) IFS=',' read -r -a t <<< "$2"; TARGETS+=("${t[@]}"); shift ;;
    --juce-source) JUCE="$2"; shift ;;
    --generator) GEN="$2"; shift ;;
    --ccache) CCACHE=1 ;;
    --copy-to-system) COPY=ON ;;
    --cmake-arg) EXTRA+=("$2"); shift ;;
    --no-configure) CONFIGURE=0 ;;
    --dry-run) DRY_RUN=1 ;;
    *) echo "unknown option $1"; exit 64 ;;
  esac
  shift
done
step_begin B-01 "Plug-in"
case "$BUILD_DIR" in /*) DIR="$BUILD_DIR" ;; *) DIR="$ROOT/$BUILD_DIR" ;; esac
case "$(cd "$(dirname "$DIR")" 2>/dev/null && pwd)/" in "$ROOT"/*) ;; *) fail "build folder $DIR is outside this repo" ;; esac
CMAKE="$(cmake_exe)"

if [ "$CONFIGURE" = 1 ]; then
  A=(-S "$ROOT" -B "$DIR" "-DCMAKE_BUILD_TYPE=$CONFIG" "-DBUILD_DEMO=$DEMO" "-DWLA_COPY_PLUGIN_AFTER_BUILD=$COPY")
  [ -n "$GEN" ] && A+=(-G "$GEN")
  [ "$UNIVERSAL" = 1 ] && A+=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
  [ "$TESTS" = 1 ] && A+=(-DWLA_TESTS=ON)
  [ "$REVIEW" = 1 ] && A+=(-DWLA_REVIEW_RENDER=ON)
  [ -n "$JUCE" ] && A+=("-DCPM_JUCE_SOURCE=$JUCE")
  [ "$CCACHE" = 1 ] && A+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
  [ ${#EXTRA[@]} -gt 0 ] && A+=("${EXTRA[@]}")
  run_logged "$CMAKE" "${A[@]}" || fail "CMake configure failed"
fi
B=(--build "$DIR" --config "$CONFIG" --parallel)
for t in ${TARGETS[@]+"${TARGETS[@]}"}; do B+=(--target "$t"); done
run_logged "$CMAKE" "${B[@]}" || fail "build failed"
[ "$DRY_RUN" = 1 ] && step_end SKIP "dry run: commands printed, nothing built"

ART="$DIR/plugin/${wPLUGIN_NAME}_artefacts/$CONFIG"
want() {   # want FORMAT TARGET-SUFFIX PATH: expected when the format is enabled and (no target list, or it is listed)
  has_format "$1" || return 1
  [ ${#TARGETS[@]} -eq 0 ] && return 0
  case " ${TARGETS[*]} " in *"_$2 "*) return 0 ;; esac
  return 1
}
BUILT=()
want VST3 VST3 && { [ -d "$ART/VST3/$wPLUGIN_NAME.vst3" ] || fail "VST3 was not built"; BUILT+=(VST3); }
want AU AU && { [ -d "$ART/AU/$wPLUGIN_NAME.component" ] || fail "AU was not built"; BUILT+=(AU); }
want Standalone Standalone && { [ -d "$ART/Standalone/$wPLUGIN_NAME.app" ] || fail "Standalone was not built"; BUILT+=(Standalone); }
ARCHS=""
[ -d "$ART/VST3/$wPLUGIN_NAME.vst3" ] && ARCHS=$(lipo -archs "$ART/VST3/$wPLUGIN_NAME.vst3/Contents/MacOS/$wPLUGIN_NAME" 2>/dev/null)
step_end PASS "built ${BUILT[*]:-targets ${TARGETS[*]}} in $BUILD_DIR ($CONFIG${ARCHS:+, $ARCHS}$([ "$DEMO" = ON ] && echo ', demo'))"

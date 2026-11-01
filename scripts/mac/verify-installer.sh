#!/bin/bash
# [Claude 2026-10-31] B-05 Verify installer (Mac): the file customers download is signed with Developer ID Installer,
# notarized and stapled, Gatekeeper accepts it, and every bundle inside is signed with Developer ID Application and the
# hardened runtime (the app with its audio-input entitlement and microphone text); then writes <pkg>.sha256, which the
# publish job checks. Moved unchanged from workflow.yml "Verify Mac Installer Is Signed, Notarized and Stapled".
# --require-signed (GitHub, when signing ran): any gap is FAIL. Without it (a desk): an unsigned package is SKIP.
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
BUILD_DIR=build; REQUIRE=0
while [ $# -gt 0 ]; do
  case "$1" in --build-dir) BUILD_DIR="$2"; shift ;; --require-signed) REQUIRE=1 ;; --dry-run) DRY_RUN=1 ;;
    *) echo "unknown option $1"; exit 64 ;; esac; shift
done
step_begin B-05 "Verify installer"
case "$BUILD_DIR" in /*) DIR="$BUILD_DIR" ;; *) DIR="$ROOT/$BUILD_DIR" ;; esac
SUFFIX="$(build_suffix "$DIR")"
PKG="$DIR/${wPLUGIN_NAME}MacInstaller_${wPROJECT_VERSION}${SUFFIX}.pkg"
[ "$DRY_RUN" = 1 ] && step_end SKIP "dry run: would verify $(basename "$PKG")"
[ -s "$PKG" ] || fail "Installer not found: $PKG"

echo "Installer signature..."
SIG=$(pkgutil --check-signature "$PKG"); echo "$SIG" | tee -a "$STEP_LOG"
if ! grep -q "Developer ID Installer" <<< "$SIG"; then
  [ "$REQUIRE" = 1 ] && fail "$PKG is not signed with a Developer ID Installer certificate"
  shasum -a 256 "$PKG" | awk '{print $1}' > "$PKG.sha256"
  step_end SKIP "unsigned desk build, nothing to verify: $(basename "$PKG") sha256 $(cat "$PKG.sha256")"
fi

echo "Notarization ticket stapled, and Gatekeeper accepts it..."
xcrun stapler validate "$PKG" || fail "$PKG has no valid stapled notarization ticket"
spctl --assess --type install -vv "$PKG" || fail "Gatekeeper does not accept $PKG"

echo "Every bundle inside the installer is signed (Developer ID Application, hardened runtime)..."
X="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/pkg-expanded"
rm -rf "$X"
pkgutil --expand-full "$PKG" "$X" || fail "could not expand $PKG"
for want in $wFORMATS; do
  case "$want" in
    VST3) ext="vst3" ;;
    AU) ext="component" ;;
    Standalone) ext="app" ;;
    *) continue ;;
  esac
  FOUND=$(find "$X" -type d -name "*.${ext}" -prune | head -n 1)
  [ -n "$FOUND" ] || fail "No .${ext} found inside $PKG (wFORMATS lists $want)"
  codesign --verify --deep --strict --verbose=2 "$FOUND" || fail "$FOUND does not verify"
  INFO=$(codesign -dv --verbose=2 "$FOUND" 2>&1)
  grep -q "Authority=Developer ID Application" <<< "$INFO" || fail "$FOUND is not signed with Developer ID Application"
  grep -q "runtime" <<< "$INFO" || fail "$FOUND is not signed with the hardened runtime"
  # V-08: the standalone keeps its audio-input entitlement and microphone text
  if [ "$ext" = "app" ] && [ -f "$ROOT/package_config/mac/standalone.entitlements" ]; then
    APP_ENT=$(codesign --display --entitlements - --xml "$FOUND" 2>/dev/null)
    grep -q "com.apple.security.device.audio-input" <<< "$APP_ENT" \
      || fail "$FOUND has no audio-input entitlement (macOS would give it no audio input)"
    /usr/libexec/PlistBuddy -c "Print :NSMicrophoneUsageDescription" "$FOUND/Contents/Info.plist" >/dev/null \
      || fail "$FOUND has no microphone permission text (NSMicrophoneUsageDescription)"
  fi
  echo "OK $(basename "$FOUND")"
done

shasum -a 256 "$PKG" | awk '{print $1}' > "$PKG.sha256"
STEP_ARTIFACTS+=("$PKG.sha256")
step_end PASS "Mac installer verified: $(basename "$PKG")  sha256 $(cat "$PKG.sha256")"

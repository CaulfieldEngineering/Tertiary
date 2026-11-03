#!/bin/bash
# [Claude 2026-10-31] B-02 Sign plug-in (Mac): Developer ID Application, hardened runtime, timestamp, for the VST3, AU and
# the standalone app (the app alone with the audio-input entitlement, V-08). Moved from workflow.yml "Sign Mac Binaries";
# the commands are unchanged.
#
# Identity, in order:
#   GitHub: MAC_APPLICATION_CERTIFICATE_BASE64 / MAC_CERTIFICATE_PASSWORD / KEYCHAIN_PASSWORD (as APPLICATION_CERTIFICATE_BASE64,
#           CERTIFICATE_PASSWORD, KEYCHAIN_PASSWORD) into a temporary keychain, as before. Missing on a v* tag: FAIL.
#   desk:   WLA_MAC_SIGN_IDENTITY, an identity already in the login keychain (the keychain is not changed).
#   --adhoc ad-hoc signature ("-"), to exercise the same commands and the entitlement check without a certificate.
#   none:   SKIP (warning), as the workflow did on staging without secrets. --dry-run prints the commands.
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
BUILD_DIR=build; CONFIG=Release; ADHOC=0
while [ $# -gt 0 ]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift ;; --config) CONFIG="$2"; shift ;;
    --adhoc) ADHOC=1 ;; --dry-run) DRY_RUN=1 ;; *) echo "unknown option $1"; exit 64 ;;
  esac; shift
done
step_begin B-02 "Sign plug-in"
case "$BUILD_DIR" in /*) DIR="$BUILD_DIR" ;; *) DIR="$ROOT/$BUILD_DIR" ;; esac
ART="$DIR/plugin/${wPLUGIN_NAME}_artefacts/$CONFIG"
TS=(--timestamp)
if [ -n "${APPLICATION_CERTIFICATE_BASE64:-}" ] && [ -n "${CERTIFICATE_PASSWORD:-}" ] && [ -n "${KEYCHAIN_PASSWORD:-}" ]; then
  IDENTITY="Developer ID Application"; MODE=keychain
elif in_ci; then
  if on_tag; then err "Signing or notarization secrets are missing on a release tag. Stopping."; step_end FAIL "signing secrets missing on a release tag"; fi
  warn "Required secrets for application signing are not set. Skipping application signing step."
  step_end SKIP "no signing secrets (staging): not signed"
elif [ -n "${WLA_MAC_SIGN_IDENTITY:-}" ]; then
  IDENTITY="$WLA_MAC_SIGN_IDENTITY"; MODE=login
elif [ "$ADHOC" = 1 ]; then
  IDENTITY="-"; MODE=adhoc; TS=(--timestamp=none)
elif [ "$DRY_RUN" = 1 ]; then
  IDENTITY="Developer ID Application"; MODE=dry
else
  warn "No signing identity here (set WLA_MAC_SIGN_IDENTITY, or use --adhoc / --dry-run). Not signed."
  step_end SKIP "no signing identity on this machine: not signed"
fi
[ "$DRY_RUN" = 1 ] && MODE=dry

echo "Starting the signing process for macOS binaries ($MODE)..." | tee -a "$STEP_LOG"
[ "$MODE" = keychain ] && keychain_open app-signing "$APPLICATION_CERTIFICATE_BASE64" "$CERTIFICATE_PASSWORD" "$KEYCHAIN_PASSWORD"
trap '[ "$MODE" = keychain ] && keychain_close' EXIT

sign_bundle() {   # sign_bundle PATH [extra codesign args]
  local p="$1"; shift
  [ "$MODE" = dry ] || [ -d "$p" ] || fail "not built: $p"
  run_logged codesign --force --sign "$IDENTITY" --verbose --deep --strict --options=runtime "${TS[@]}" "$@" "$p" || fail "codesign failed: $p"
  run_logged codesign --verify --deep --verbose=2 "$p" || fail "signature does not verify: $p"
  run_logged codesign --display --verbose=2 "$p" || fail "codesign --display failed: $p"
}
SIGNED=()
if has_format VST3; then sign_bundle "$ART/VST3/$wPLUGIN_NAME.vst3"; SIGNED+=(VST3); fi
if has_format AU; then sign_bundle "$ART/AU/$wPLUGIN_NAME.component"; SIGNED+=(AU); fi
if has_format Standalone; then
  APP_PATH="$ART/Standalone/$wPLUGIN_NAME.app"
  APP_ENTITLEMENTS="$ROOT/package_config/mac/standalone.entitlements"
  if [ ! -f "$APP_ENTITLEMENTS" ]; then   # a product without the V-08 entitlements file: signed like the plug-ins
    sign_bundle "$APP_PATH"
  else
  run_logged plutil -lint "$APP_ENTITLEMENTS" || fail "entitlements file is not valid: $APP_ENTITLEMENTS"
  sign_bundle "$APP_PATH" --entitlements "$APP_ENTITLEMENTS"
  fi
  if [ "$MODE" != dry ] && [ -f "$APP_ENTITLEMENTS" ]; then
    APP_ENT=$(codesign --display --entitlements - --xml "$APP_PATH" 2>/dev/null)
    grep -q "com.apple.security.device.audio-input" <<< "$APP_ENT" || fail "$APP_PATH was signed without the audio-input entitlement"
    /usr/libexec/PlistBuddy -c "Print :NSMicrophoneUsageDescription" "$APP_PATH/Contents/Info.plist" >/dev/null \
      || fail "$APP_PATH has no microphone permission text (NSMicrophoneUsageDescription)"
  fi
  SIGNED+=(Standalone)
fi
echo "Signing process completed ($MODE)." | tee -a "$STEP_LOG"
case "$MODE" in
  dry) step_end SKIP "dry run: would sign ${SIGNED[*]} with '$IDENTITY'" ;;
  adhoc) step_end PASS "ad-hoc signed and verified ${SIGNED[*]} (hardened runtime; app entitlement checked); not a Developer ID signature" ;;
  *) step_end PASS "signed and verified ${SIGNED[*]} with '$IDENTITY'" ;;
esac

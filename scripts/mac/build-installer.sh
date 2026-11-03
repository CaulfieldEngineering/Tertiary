#!/bin/bash
# [Claude 2026-10-31] B-03 Installer (Mac): component packages for the Standalone, VST3 and AU (not relocatable), each
# signed with Developer ID Installer, then <build>/<Name>MacInstaller_<version>[.D].pkg from the distribution XML, signed
# the same way. Moved from workflow.yml "Create Standalone / VST3 / AU Package", "Sign Individual Packages",
# "Create Distribution Package" and "Sign Distribution Package"; the commands are unchanged.
#
# Installer identity: on GitHub MAC_INSTALLER_CERTIFICATE_BASE64 / MAC_CERTIFICATE_PASSWORD / KEYCHAIN_PASSWORD (as
# INSTALLER_CERTIFICATE_BASE64, CERTIFICATE_PASSWORD, KEYCHAIN_PASSWORD) in a temporary keychain, as before; missing on a
# v* tag is FAIL, on staging a warning and unsigned packages. At a desk WLA_MAC_INSTALLER_IDENTITY (login keychain),
# else unsigned packages (reported in the result). --dry-run prints the commands.
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
BUILD_DIR=build; CONFIG=Release
while [ $# -gt 0 ]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift ;; --config) CONFIG="$2"; shift ;;
    --dry-run) DRY_RUN=1 ;; *) echo "unknown option $1"; exit 64 ;;
  esac; shift
done
step_begin B-03 "Installer"
case "$BUILD_DIR" in /*) DIR="$BUILD_DIR" ;; *) DIR="$ROOT/$BUILD_DIR" ;; esac
ART="$DIR/plugin/${wPLUGIN_NAME}_artefacts/$CONFIG"
NAME="$wPLUGIN_NAME"; VERSION="$wPROJECT_VERSION"
# Package identifiers com.<company>.<name>.<kind>: wCOMPANY_ID when config.cmake has it (the template's way), else the
# company name without spaces (Tertiary's workflow; for Tertiary both are "WonderlandAudio")
FORMATTED_COMPANY_NAME="${wCOMPANY_ID:-$(echo "$wCOMPANY_NAME" | tr -d ' ')}"
SUFFIX="$(build_suffix "$DIR")"
OUT="$DIR/output"
TMPD="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
run_logged mkdir -p "$OUT"

if [ -n "${INSTALLER_CERTIFICATE_BASE64:-}" ] && [ -n "${CERTIFICATE_PASSWORD:-}" ] && [ -n "${KEYCHAIN_PASSWORD:-}" ]; then
  SIGN_ID="Developer ID Installer"; MODE=keychain
elif in_ci; then
  if on_tag; then err "Signing or notarization secrets are missing on a release tag. Stopping."; step_end FAIL "installer signing secrets missing on a release tag"; fi
  warn "Required secrets for installer signing are not set. Skipping installer signing step."
  MODE=unsigned
elif [ -n "${WLA_MAC_INSTALLER_IDENTITY:-}" ]; then
  SIGN_ID="$WLA_MAC_INSTALLER_IDENTITY"; MODE=login
else
  MODE=unsigned
fi
[ "$DRY_RUN" = 1 ] && [ "$MODE" = unsigned ] && SIGN_ID="Developer ID Installer"
[ "$MODE" = keychain ] && [ "$DRY_RUN" != 1 ] && keychain_open pkg-signing "$INSTALLER_CERTIFICATE_BASE64" "$CERTIFICATE_PASSWORD" "$KEYCHAIN_PASSWORD"
trap '[ "$MODE" = keychain ] && keychain_close' EXIT

component() {   # component FORMAT BUNDLE-PATH IDENTIFIER-SUFFIX INSTALL-LOCATION
  local fmt="$1" path="$2" ident="$3" loc="$4" pkg="$OUT/${NAME}_$1_${VERSION}${SUFFIX}.pkg"
  if [ "$DRY_RUN" != 1 ] && [ ! -d "$path" ]; then
    echo "ERROR: $fmt bundle not found at: $path"; ls -la "$(dirname "$path")" 2>&1
    fail "$fmt bundle not found at: $path"
  fi
  # Not relocatable: otherwise, if macOS knows another copy of this bundle anywhere (an old download, a build folder),
  # the installer updates that copy and the right folder stays empty (found by installer-test.yml on the Intel Mac)
  run_logged pkgbuild --analyze --root "$(dirname "$path")" "$TMPD/component.plist" || fail "pkgbuild --analyze failed ($fmt)"
  run_logged plutil -replace BundleIsRelocatable -bool NO "$TMPD/component.plist" || fail "plutil failed ($fmt)"
  run_logged pkgbuild --root "$(dirname "$path")" --component-plist "$TMPD/component.plist" \
    --identifier "com.${FORMATTED_COMPANY_NAME}.${NAME}.${ident}" --version "$VERSION" --install-location "$loc" "$pkg" \
    || fail "pkgbuild failed ($fmt)"
  [ "$DRY_RUN" = 1 ] || [ -s "$pkg" ] || fail "ERROR: $fmt package was not created, or is empty: $pkg"
  if [ "$MODE" != unsigned ] || [ "$DRY_RUN" = 1 ]; then
    run_logged productsign --sign "$SIGN_ID" "$pkg" "${pkg%.pkg}_signed.pkg" || fail "productsign failed ($fmt)"
    run_logged pkgutil --check-signature "${pkg%.pkg}_signed.pkg" || fail "package signature does not verify ($fmt)"
    run_logged mv "${pkg%.pkg}_signed.pkg" "$pkg"
  fi
  PKGS+=("$fmt")
}
PKGS=()
has_format Standalone && component Standalone "$ART/Standalone/$NAME.app" app "/Applications/${NAME}"
has_format VST3 && component VST3 "$ART/VST3/$NAME.vst3" vst3 "/Library/Audio/Plug-Ins/VST3"
has_format AU && component AU "$ART/AU/$NAME.component" component "/Library/Audio/Plug-Ins/Components"

DIST_FILE="distribution-full.xml"; [ "$SUFFIX" = ".D" ] && DIST_FILE="distribution-demo.xml"
echo "Using distribution file: $DIST_FILE" | tee -a "$STEP_LOG"
[ "$DRY_RUN" = 1 ] || [ -f "$DIR/$DIST_FILE" ] || fail "Distribution file not found: $DIR/$DIST_FILE (configure with CMake first)"
run_logged cp "$DIR/$DIST_FILE" "$OUT/distribution.xml" || fail "Failed to copy distribution file"
FINAL="$DIR/${NAME}MacInstaller_${VERSION}${SUFFIX}.pkg"
run_logged productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$FINAL" || fail "productbuild failed"
[ "$DRY_RUN" = 1 ] || [ -f "$FINAL" ] || fail "Failed to create distribution package"
if [ "$MODE" != unsigned ] || [ "$DRY_RUN" = 1 ]; then
  run_logged productsign --sign "$SIGN_ID" "$FINAL" "${FINAL%.pkg}_signed.pkg" || fail "productsign failed (distribution)"
  run_logged pkgutil --check-signature "${FINAL%.pkg}_signed.pkg" || fail "distribution signature does not verify"
  run_logged mv "${FINAL%.pkg}_signed.pkg" "$FINAL"
fi
[ "$DRY_RUN" = 1 ] && step_end SKIP "dry run: would package ${PKGS[*]} into $(basename "$FINAL"), signed with '$SIGN_ID'"
SIZE=$(du -h "$FINAL" | cut -f1)
if [ "$MODE" = unsigned ]; then
  step_end PASS "$(basename "$FINAL") ($SIZE; ${PKGS[*]}), NOT signed: no installer identity here"
fi
step_end PASS "$(basename "$FINAL") ($SIZE; ${PKGS[*]}), signed with '$SIGN_ID'"

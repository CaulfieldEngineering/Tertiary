#!/bin/bash
# [Claude 2026-10-31] B-04 Notarize (Mac): submit <build>/<Name>MacInstaller_<version>[.D].pkg to Apple's notary service,
# wait (at most 15 minutes), then staple the ticket and check it with Gatekeeper. Moved from workflow.yml "Notarize Mac
# Distribution Package" and "Staple Notarization Ticket"; the commands are unchanged.
# Needs APPLE_ID, APPLE_TEAM_ID and APPLE_APP_SPECIFIC_PASSWORD (GitHub secrets, or a desk's environment). Missing on a
# v* tag: FAIL; otherwise a warning and SKIP, as before. --dry-run prints the commands.
. "$(cd -- "$(dirname -- "$0")" && pwd)/../lib/common.sh"
BUILD_DIR=build
while [ $# -gt 0 ]; do
  case "$1" in --build-dir) BUILD_DIR="$2"; shift ;; --dry-run) DRY_RUN=1 ;; *) echo "unknown option $1"; exit 64 ;; esac; shift
done
step_begin B-04 "Notarize installer"
case "$BUILD_DIR" in /*) DIR="$BUILD_DIR" ;; *) DIR="$ROOT/$BUILD_DIR" ;; esac
SUFFIX="$(build_suffix "$DIR")"
INSTALLER_PATH="$DIR/${wPLUGIN_NAME}MacInstaller_${wPROJECT_VERSION}${SUFFIX}.pkg"

if [ -z "${APPLE_ID:-}" ] || [ -z "${APPLE_TEAM_ID:-}" ] || [ -z "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]; then
  if on_tag; then err "Signing or notarization secrets are missing on a release tag. Stopping."; step_end FAIL "notarization secrets missing on a release tag"; fi
  if [ "$DRY_RUN" = 1 ]; then
    for c in "xcrun notarytool store-credentials notarytool-profile --apple-id ... --team-id ... --password ..." \
             "xcrun notarytool submit $INSTALLER_PATH --keychain-profile notarytool-profile" \
             "xcrun stapler staple $INSTALLER_PATH" "xcrun stapler validate $INSTALLER_PATH" "spctl --assess --type install $INSTALLER_PATH"; do
      echo "> $c" | tee -a "$STEP_LOG"
    done
    step_end SKIP "dry run: would notarize and staple $(basename "$INSTALLER_PATH")"
  fi
  warn "Required secrets for notarization are not set. Skipping notarization step."
  step_end SKIP "no notarization credentials here: not notarized"
fi
[ "$DRY_RUN" = 1 ] && step_end SKIP "dry run: credentials present; would notarize and staple $(basename "$INSTALLER_PATH")"
[ -s "$INSTALLER_PATH" ] || fail "Installer not found: $INSTALLER_PATH"

echo "Starting the notarization process..."
xcrun notarytool store-credentials "notarytool-profile" \
  --apple-id "$APPLE_ID" \
  --team-id "$APPLE_TEAM_ID" \
  --password "$APPLE_APP_SPECIFIC_PASSWORD" || fail "notarytool store-credentials failed"

echo "Submitting for notarization..."
SUBMISSION_RESPONSE=$(xcrun notarytool submit "$INSTALLER_PATH" --keychain-profile "notarytool-profile")
SUBMISSION_ID=$(echo "$SUBMISSION_RESPONSE" | grep "id:" | head -n 1 | awk '{print $2}')
if [ -z "$SUBMISSION_ID" ]; then
  echo "Full submission response: $SUBMISSION_RESPONSE"
  fail "Failed to get submission ID"
fi
echo "Captured Submission ID: $SUBMISSION_ID"

ATTEMPTS=0
MAX_ATTEMPTS=30  # 15 minutes maximum (30 * 30 seconds)
while [ $ATTEMPTS -lt $MAX_ATTEMPTS ]; do
  echo "Checking status for submission ID: $SUBMISSION_ID"
  STATUS_RESPONSE=$(xcrun notarytool info "$SUBMISSION_ID" --keychain-profile "notarytool-profile")
  echo "Full status response: $STATUS_RESPONSE"
  STATUS=$(echo "$STATUS_RESPONSE" | grep "status:" | awk '{print $2}')
  echo "Parsed status: $STATUS"
  if [ "$STATUS" = "Accepted" ]; then
    echo "Notarization successful!"
    break
  elif [ "$STATUS" = "Invalid" ] || [ "$STATUS" = "Rejected" ]; then
    xcrun notarytool log "$SUBMISSION_ID" --keychain-profile "notarytool-profile"
    fail "Notarization failed with status: $STATUS"
  fi
  ATTEMPTS=$((ATTEMPTS + 1))
  if [ $ATTEMPTS -eq $MAX_ATTEMPTS ]; then
    fail "Timeout waiting for notarization after 15 minutes"
  fi
  echo "Waiting for notarization to complete... (Attempt $ATTEMPTS of $MAX_ATTEMPTS)"
  sleep 30
done
xcrun notarytool history --keychain-profile "notarytool-profile"

echo "Stapling notarization ticket to installer..."
xcrun stapler staple "$INSTALLER_PATH" || fail "stapler staple failed"
echo "Validating stapled installer..."
xcrun stapler validate "$INSTALLER_PATH" || fail "stapler validate failed"
spctl --assess --type install "$INSTALLER_PATH" || fail "Gatekeeper does not accept the installer"
step_end PASS "notarized (submission $SUBMISSION_ID), stapled and accepted by Gatekeeper: $(basename "$INSTALLER_PATH")"

#!/bin/bash
# build_unsigned_installer_mac.sh
# Written by Claude (2026-10-27). Self-test only (installer-test.yml on a claude/installer-test branch):
# builds the Mac installer the same way workflow.yml does (component packages, then productbuild with
# distribution-full.xml), but UNSIGNED, so the install and uninstall checks can be tried before anything
# goes to staging. The real test uses the signed, notarized installer from the staging pipeline.
#
#   bash build_unsigned_installer_mac.sh <cmake build folder> <output folder>

set -euo pipefail
BUILD="$1"
OUT="$2"
NAME=Tertiary
VERSION=$(grep 'set(wPROJECT_VERSION' config.cmake | sed -E 's/.*"(.*)".*/\1/')
ART="$BUILD/plugin/${NAME}_artefacts/Release"
mkdir -p "$OUT" "$BUILD/output"

# Not relocatable, as in workflow.yml (else the installer may update another copy it knows of, e.g. in build/)
plist() { pkgbuild --analyze --root "$1" "$BUILD/output/$2.plist" >/dev/null; plutil -replace BundleIsRelocatable -bool NO "$BUILD/output/$2.plist"; }
plist "$ART/Standalone" app; plist "$ART/VST3" vst3; plist "$ART/AU" au

# Same identifiers and install locations as workflow.yml
pkgbuild --component-plist "$BUILD/output/app.plist" --root "$ART/Standalone" --identifier "com.WonderlandAudio.$NAME.app" --version "$VERSION" \
         --install-location "/Applications/$NAME" "$BUILD/output/${NAME}_Standalone_${VERSION}.pkg"
pkgbuild --component-plist "$BUILD/output/vst3.plist" --root "$ART/VST3" --identifier "com.WonderlandAudio.$NAME.vst3" --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" "$BUILD/output/${NAME}_VST3_${VERSION}.pkg"
pkgbuild --component-plist "$BUILD/output/au.plist" --root "$ART/AU" --identifier "com.WonderlandAudio.$NAME.component" --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" "$BUILD/output/${NAME}_AU_${VERSION}.pkg"

cp "$BUILD/distribution-full.xml" "$BUILD/output/distribution.xml"
productbuild --distribution "$BUILD/output/distribution.xml" --package-path "$BUILD/output" \
             "$OUT/${NAME}MacInstaller_${VERSION}.pkg"
ls -la "$OUT"

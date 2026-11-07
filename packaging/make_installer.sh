#!/bin/bash
# Builds "QUARRY Installer.pkg" (Standalone + VST3 + AU, all-in-one) and wraps it in a DMG.
# Usage: packaging/make_installer.sh <build-dir> <version>
# Optional env: INSTALLER_SIGN_ID="Developer ID Installer: ..." to sign the package.
set -euo pipefail

BUILD="${1:-build}"
VERSION="${2:-1.0.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PK="$ROOT/packaging"
WORK="$(mktemp -d)"
OUT="$ROOT/dist"
ID="com.kalabash.quarry"
mkdir -p "$OUT"

REL="Release"
APP="$BUILD/Quarry_artefacts/$REL/Standalone/QUARRY.app"
VST3_A="$BUILD/Quarry_artefacts/$REL/VST3/QUARRY.vst3"
VST3_B="$BUILD/QuarryFX_artefacts/$REL/VST3/QUARRY FX.vst3"
AU_A="$BUILD/Quarry_artefacts/$REL/AU/QUARRY.component"
AU_B="$BUILD/QuarryFX_artefacts/$REL/AU/QUARRY FX.component"
for f in "$APP" "$VST3_A" "$VST3_B" "$AU_A" "$AU_B"; do [ -e "$f" ] || { echo "missing: $f"; exit 1; }; done

# component package: name, install location, bundles...
component() {
    local name="$1" dest="$2"; shift 2
    local root="$WORK/root-$name"
    mkdir -p "$root"
    for b in "$@"; do ditto "$b" "$root/$(basename "$b")"; done
    pkgbuild --analyze --root "$root" "$WORK/$name.plist"
    # always install into the fixed location (never "relocate" to an old copy elsewhere on disk)
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/$name.plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$name.plist"
        i=$((i + 1))
    done
    pkgbuild --root "$root" --component-plist "$WORK/$name.plist" \
             --identifier "$ID.$name" --version "$VERSION" \
             --install-location "$dest" --scripts "$PK/scripts" \
             "$WORK/$name.pkg"
}

component standalone "/Applications"                      "$APP"
component vst3       "/Library/Audio/Plug-Ins/VST3"       "$VST3_A" "$VST3_B"
component au         "/Library/Audio/Plug-Ins/Components" "$AU_A" "$AU_B"

sed "s/@VERSION@/$VERSION/g" "$PK/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$WORK/resources"
for f in "$PK"/resources/*; do sed "s/@VERSION@/$VERSION/g" "$f" > "$WORK/resources/$(basename "$f")"; done

UNSIGNED="$WORK/QUARRY Installer.pkg"
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" \
             --package-path "$WORK" --version "$VERSION" "$UNSIGNED"

PKG="$OUT/QUARRY Installer.pkg"
if [ -n "${INSTALLER_SIGN_ID:-}" ]; then
    productsign --sign "$INSTALLER_SIGN_ID" "$UNSIGNED" "$PKG"
else
    cp "$UNSIGNED" "$PKG"
fi

# DMG holding the installer + a short read-me
DMGDIR="$WORK/dmg"; mkdir -p "$DMGDIR"
cp "$PKG" "$DMGDIR/"
cp "$WORK/resources/readme.txt" "$DMGDIR/Read Me.txt"
hdiutil create -volname "QUARRY $VERSION" -srcfolder "$DMGDIR" -ov -format UDZO "$OUT/QUARRY-$VERSION-mac.dmg"
echo "Built: $PKG"
echo "Built: $OUT/QUARRY-$VERSION-mac.dmg"

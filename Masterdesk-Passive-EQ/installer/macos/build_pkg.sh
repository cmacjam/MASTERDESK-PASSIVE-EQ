#!/bin/bash
# Builds "MasterDesk-Mastering-Suite-<version>-macOS.pkg" containing VST3, AU and the
# standalone app, each selectable in the installer.
#
#   installer/macos/build_pkg.sh <artefacts-dir> <version> <output-dir>
#   e.g. installer/macos/build_pkg.sh build/PassiveMasterEQ_artefacts/Release 1.0.0 dist
#
# Optional signing (otherwise bundles are ad-hoc signed, fine for your own Macs):
#   MACOS_APP_IDENTITY        "Developer ID Application: Your Name (TEAMID)"
#   MACOS_INSTALLER_IDENTITY  "Developer ID Installer: Your Name (TEAMID)"
# Optional notarization (needs both identities above):
#   APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD (app-specific password)
set -euo pipefail

ART="$1"; VERSION="$2"; OUT="$3"
NAME="MasterDesk Mastering Suite"
ID="com.basemental.masterdeskpassiveeq"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app" "$STAGE/pkgs"
cp -R "$ART/VST3/$NAME.vst3"       "$STAGE/vst3/"
cp -R "$ART/AU/$NAME.component"    "$STAGE/au/"
cp -R "$ART/Standalone/$NAME.app"  "$STAGE/app/"

# --- code signing -------------------------------------------------------------
SIGN_ID="${MACOS_APP_IDENTITY:--}"
for b in "$STAGE/vst3/$NAME.vst3" "$STAGE/au/$NAME.component" "$STAGE/app/$NAME.app"; do
    if [ "$SIGN_ID" = "-" ]; then
        codesign --force --deep --sign - "$b"
    else
        codesign --force --deep --options runtime --timestamp --sign "$SIGN_ID" "$b"
    fi
done

# --- component packages (bundles must not be "relocated" by the installer) -----
make_pkg () {   # <root> <id-suffix> <install-location>
    local root="$1" suffix="$2" location="$3"
    local plist="$STAGE/$suffix.plist"
    pkgbuild --analyze --root "$root" "$plist"
    # Mark every bundle pkgbuild found as non-relocatable (so the installer never
    # "finds" an old copy elsewhere and updates that instead). pkgbuild doesn't
    # always recognise .vst3 bundles, so the list can be empty - that's fine.
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
        i=$((i + 1))
    done
    echo "  $suffix: $i bundle(s) marked non-relocatable"
    pkgbuild --root "$root" --component-plist "$plist" --identifier "$ID.$suffix" \
             --version "$VERSION" --install-location "$location" "$STAGE/pkgs/$suffix.pkg"
}
make_pkg "$STAGE/vst3" vst3 "/Library/Audio/Plug-Ins/VST3"
make_pkg "$STAGE/au"   au   "/Library/Audio/Plug-Ins/Components"
make_pkg "$STAGE/app"  app  "/Applications"

# --- installer with selectable components ---------------------------------------
cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$NAME $VERSION</title>
    <welcome file="welcome.txt" mime-type="text/plain"/>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plug-in" description="For Cubase, Ableton Live, Studio One, Reaper, FL Studio, Bitwig and other VST3 hosts.">
        <pkg-ref id="$ID.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU)" description="For Logic Pro, GarageBand and other AU hosts.">
        <pkg-ref id="$ID.au"/>
    </choice>
    <choice id="app" title="Standalone app" description="Runs on its own without a DAW (Applications folder).">
        <pkg-ref id="$ID.app"/>
    </choice>
    <pkg-ref id="$ID.vst3" version="$VERSION">vst3.pkg</pkg-ref>
    <pkg-ref id="$ID.au"   version="$VERSION">au.pkg</pkg-ref>
    <pkg-ref id="$ID.app"  version="$VERSION">app.pkg</pkg-ref>
</installer-gui-script>
EOF

# installer welcome text (created here if the resources folder is missing)
RES="$HERE/resources"
if [ ! -f "$RES/welcome.txt" ]; then
    RES="$STAGE/resources"; mkdir -p "$RES"
    printf '%s\n' "Basemental MasterDesk Mastering Suite" "" \
        "Choose what to install: VST3 plug-in, Audio Unit (Logic, GarageBand) and/or the standalone app." "" \
        "Afterwards, rescan plug-ins in your DAW. Inside the plug-in, press HELP ? for a guide." > "$RES/welcome.txt"
fi

PKG="$OUT/MasterDesk-Mastering-Suite-$VERSION-macOS.pkg"
UNSIGNED="$STAGE/unsigned.pkg"
productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
             --resources "$RES" "$UNSIGNED"

if [ -n "${MACOS_INSTALLER_IDENTITY:-}" ]; then
    productsign --sign "$MACOS_INSTALLER_IDENTITY" "$UNSIGNED" "$PKG"
    if [ -n "${APPLE_ID:-}" ] && [ -n "${APPLE_TEAM_ID:-}" ] && [ -n "${APPLE_APP_PASSWORD:-}" ]; then
        xcrun notarytool submit "$PKG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
              --password "$APPLE_APP_PASSWORD" --wait
        xcrun stapler staple "$PKG"
    fi
else
    cp "$UNSIGNED" "$PKG"
fi
echo "Built $PKG"

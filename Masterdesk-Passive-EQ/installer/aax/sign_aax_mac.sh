#!/bin/bash
# Sign the Pro Tools (AAX) plug-in with PACE's wraptool so retail Pro Tools will load it.
# Needs: PACE Eden tools (wraptool) from your PACE developer account, your iLok plugged in
#        (or iLok Cloud), and an Apple "Developer ID Application" certificate in your keychain.
# Check the flags against the wraptool guide that comes with PACE's tools.
#
# Usage:  ./sign_aax_mac.sh  <PACE account>  <wrap config GUID>  "<Developer ID Application: Name (TEAMID)>"  [path to .aaxplugin]
set -euo pipefail
ACCOUNT="${1:?PACE account name}"
WCGUID="${2:?wrap configuration GUID from PACE}"
SIGNID="${3:?Apple signing identity, e.g. Developer ID Application: Your Name (TEAMID)}"
AAX="${4:-/Library/Application Support/Avid/Audio/Plug-Ins/MasterDesk Mastering Suite.aaxplugin}"
WRAPTOOL="${WRAPTOOL:-$(command -v wraptool || echo /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool)}"

[ -d "$AAX" ] || { echo "Can't find $AAX"; exit 1; }
[ -x "$WRAPTOOL" ] || { echo "Can't find wraptool. Install PACE Eden tools or set WRAPTOOL=/path/to/wraptool"; exit 1; }
echo "Signing $AAX"
sudo "$WRAPTOOL" sign --verbose --account "$ACCOUNT" --wcguid "$WCGUID" --signid "$SIGNID" --in "$AAX" --out "$AAX"
"$WRAPTOOL" verify --verbose --in "$AAX"
echo "Done. Start Pro Tools: the plug-in appears under EQ and Dynamics."

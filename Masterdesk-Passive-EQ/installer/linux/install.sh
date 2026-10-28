#!/bin/bash
# Installs Basemental MasterDesk Mastering Suite for the current user (no root needed):
#   VST3       -> ~/.vst3
#   Standalone -> ~/.local/bin
# Run from the extracted folder:  ./install.sh      (./install.sh --uninstall to remove)
set -euo pipefail
NAME="MasterDesk Mastering Suite"
OLD="Masterdesk Passive EQ"            # name used up to version 1.3.0
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ "${1:-}" = "--uninstall" ]; then
    rm -rf "$HOME/.vst3/$NAME.vst3" "$HOME/.local/bin/$NAME"
    echo "Removed $NAME."
    exit 0
fi

mkdir -p "$HOME/.vst3" "$HOME/.local/bin"
rm -rf "$HOME/.vst3/$NAME.vst3" "$HOME/.vst3/$OLD.vst3" "$HOME/.local/bin/$OLD"
cp -R "$HERE/$NAME.vst3" "$HOME/.vst3/"
echo "Installed:"
echo "  VST3:       $HOME/.vst3/$NAME.vst3"
if [ -f "$HERE/$NAME" ]; then
    cp "$HERE/$NAME" "$HOME/.local/bin/"
    chmod +x "$HOME/.local/bin/$NAME"
    echo "  Standalone: $HOME/.local/bin/$NAME"
fi
echo "Rescan plug-ins in your DAW (Reaper, Bitwig, Ardour...)."

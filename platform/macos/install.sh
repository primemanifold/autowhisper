#!/usr/bin/env bash
# AutoWhisper macOS installer (dev-mode, unsigned-OK).
# Expects the build to have produced:
#   build/AutoWhisper.app
#   build/us.primemanifold.autowhisper.plist
#
# Installs to /Applications and loads a user LaunchAgent.

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
APP="${BUILD_DIR}/AutoWhisper.app"
PLIST_SRC="${BUILD_DIR}/us.primemanifold.autowhisper.plist"

if [[ ! -d "$APP" ]]; then
    echo "Error: $APP not found. Run: cmake --build $BUILD_DIR --target autowhisper_bundle"
    exit 1
fi

if [[ ! -f "$PLIST_SRC" ]]; then
    echo "Error: $PLIST_SRC not found. Reconfigure cmake."
    exit 1
fi

echo "Installing AutoWhisper.app to /Applications (requires sudo)..."
sudo rm -rf /Applications/AutoWhisper.app
sudo cp -R "$APP" /Applications/AutoWhisper.app

echo "Creating /usr/local/bin/autowhisper symlink..."
sudo mkdir -p /usr/local/bin
sudo ln -sf /Applications/AutoWhisper.app/Contents/MacOS/autowhisper /usr/local/bin/autowhisper

echo "Installing LaunchAgent plist..."
mkdir -p "$HOME/Library/LaunchAgents"
cp "$PLIST_SRC" "$HOME/Library/LaunchAgents/us.primemanifold.autowhisper.plist"

mkdir -p "$HOME/Library/Logs/autowhisper"

echo ""
echo "Installed."
echo ""
echo "Next steps:"
echo "  1. autowhisper doctor          # check permissions"
echo "  2. Grant the 3 TCC permissions System Settings will ask for:"
echo "     - Input Monitoring"
echo "     - Accessibility"
echo "     - Microphone"
echo "  3. autowhisper start           # load the LaunchAgent"
echo ""

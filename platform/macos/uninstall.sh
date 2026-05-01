#!/usr/bin/env bash
# AutoWhisper macOS uninstaller.

set -euo pipefail

LABEL="us.primemanifold.autowhisper"

echo "Stopping and unloading LaunchAgent..."
launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true

echo "Removing LaunchAgent plist..."
rm -f "$HOME/Library/LaunchAgents/$LABEL.plist"

echo "Removing /Applications/AutoWhisper.app..."
sudo rm -rf /Applications/AutoWhisper.app

echo "Removing /usr/local/bin/autowhisper symlink..."
sudo rm -f /usr/local/bin/autowhisper

echo ""
echo "Uninstalled. Logs at ~/Library/Logs/autowhisper/ are preserved."
echo "Config at ~/.config/autowhisper/ is preserved."
echo ""
echo "To revoke TCC permissions: System Settings \u2192 Privacy & Security."

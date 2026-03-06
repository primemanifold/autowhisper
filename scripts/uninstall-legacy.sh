#!/usr/bin/env bash
# uninstall-legacy.sh — Remove the old Python-based AutoWhisper installation
# The C++ rewrite installs via PPA to /usr/bin/autowhisper. This script cleans
# up the legacy /opt/autowhisper layout (venv, Python source, old service file).

set -euo pipefail

echo "=== AutoWhisper Legacy (Python) Uninstaller ==="
echo

# 1. Stop and disable the old systemd user service
if systemctl --user is-active autowhisper &>/dev/null; then
    echo "Stopping autowhisper user service..."
    systemctl --user stop autowhisper
fi

if systemctl --user is-enabled autowhisper &>/dev/null; then
    echo "Disabling autowhisper user service..."
    systemctl --user disable autowhisper
fi

# 2. Remove the old service file (user-level copy)
OLD_SERVICE="$HOME/.config/systemd/user/autowhisper.service"
if [[ -f "$OLD_SERVICE" ]]; then
    echo "Removing old service file: $OLD_SERVICE"
    rm -f "$OLD_SERVICE"
fi

# 3. Reload systemd so it forgets the unit
systemctl --user daemon-reload

# 4. Remove /opt/autowhisper (venv, Python source, old config)
if [[ -d /opt/autowhisper ]]; then
    echo "Removing /opt/autowhisper..."
    sudo rm -rf /opt/autowhisper
fi

echo
echo "Legacy Python installation removed."
echo
echo "Next steps — install the C++ version via PPA:"
echo "  sudo add-apt-repository ppa:primemanifold/autowhisper"
echo "  sudo apt update && sudo apt install autowhisper"
echo "  systemctl --user enable --now autowhisper"

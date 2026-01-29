#!/bin/bash
# AutoWhisper Daemon Setup Script
# Sets up the systemd user service for AutoWhisper

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

INSTALL_DIR="${AUTOWHISPER_DIR:-/opt/autowhisper}"
SERVICE_FILE="$INSTALL_DIR/autowhisper.service"
USER_SERVICE_DIR="$HOME/.config/systemd/user"

# Check if running as root (should NOT be)
if [[ $EUID -eq 0 ]]; then
    error "Do NOT run this script as root or with sudo!"
    error "The daemon runs as your user within your graphical session."
    exit 1
fi

# Check if in graphical session
if [[ -z "$DISPLAY" && -z "$WAYLAND_DISPLAY" ]]; then
    error "No graphical session detected (DISPLAY/WAYLAND_DISPLAY not set)."
    error "Please run this from within your desktop environment."
    exit 1
fi

# Check if service file exists
if [[ ! -f "$SERVICE_FILE" ]]; then
    error "Service file not found: $SERVICE_FILE"
    error "Please run the installer first: sudo ./install.sh"
    exit 1
fi

info "Setting up AutoWhisper daemon..."

# Create user systemd directory
mkdir -p "$USER_SERVICE_DIR"

# Copy service file
cp "$SERVICE_FILE" "$USER_SERVICE_DIR/"
info "Installed service file to $USER_SERVICE_DIR/"

# Reload systemd user daemon
systemctl --user daemon-reload
info "Reloaded systemd user daemon"

# Enable the service (start on login)
systemctl --user enable autowhisper.service
info "Enabled autowhisper service (will start on login)"

# Start the service
systemctl --user start autowhisper.service
info "Started autowhisper service"

# Check status
echo ""
info "Service status:"
systemctl --user status autowhisper.service --no-pager || true

echo ""
echo "=========================================="
echo -e "${GREEN}Daemon Setup Complete!${NC}"
echo "=========================================="
echo ""
echo "Commands:"
echo "  Status:   systemctl --user status autowhisper"
echo "  Logs:     journalctl --user -u autowhisper -f"
echo "  Stop:     systemctl --user stop autowhisper"
echo "  Restart:  systemctl --user restart autowhisper"
echo "  Disable:  systemctl --user disable autowhisper"
echo ""
echo "The daemon will automatically start when you log in."
echo ""

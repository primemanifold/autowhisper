#!/bin/bash
# AutoWhisper Daemon Restart Script
# Restarts the systemd user service

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check if running as root (should NOT be)
if [[ $EUID -eq 0 ]]; then
    error "Do NOT run this script as root or with sudo!"
    exit 1
fi

# Check if service is installed
if ! systemctl --user list-unit-files | grep -q "autowhisper.service"; then
    error "AutoWhisper service not installed."
    error "Run: ./scripts/setup-daemon.sh"
    exit 1
fi

info "Restarting AutoWhisper daemon..."

# Reload daemon config (in case service file changed)
systemctl --user daemon-reload

# Restart the service
systemctl --user restart autowhisper.service

# Wait a moment for startup
sleep 1

# Check status
info "Service status:"
systemctl --user status autowhisper.service --no-pager || true

echo ""
info "Daemon restarted successfully!"
echo ""
echo "View logs: journalctl --user -u autowhisper -f"

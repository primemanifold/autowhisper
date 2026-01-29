#!/bin/bash
# AutoWhisper Daemon Status Script
# Shows the status of the systemd user service

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

# Check if running as root
if [[ $EUID -eq 0 ]]; then
    echo -e "${RED}[ERROR]${NC} Do NOT run this script as root or with sudo!"
    exit 1
fi

echo "=========================================="
echo "AutoWhisper Daemon Status"
echo "=========================================="
echo ""

# Check if service is installed
if ! systemctl --user list-unit-files 2>/dev/null | grep -q "autowhisper.service"; then
    warn "AutoWhisper service not installed."
    warn "Run: ./scripts/setup-daemon.sh"
    exit 1
fi

# Show service status
systemctl --user status autowhisper.service --no-pager 2>/dev/null || true

echo ""
echo "=========================================="
echo "Recent Logs (last 20 lines)"
echo "=========================================="
journalctl --user -u autowhisper -n 20 --no-pager 2>/dev/null || echo "(no logs available)"

echo ""
echo "Commands:"
echo "  Full logs:  journalctl --user -u autowhisper -f"
echo "  Restart:    ./scripts/restart-daemon.sh"
echo "  Stop:       ./scripts/stop-daemon.sh"

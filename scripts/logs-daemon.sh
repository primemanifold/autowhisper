#!/bin/bash
# AutoWhisper Daemon Logs Script
# Shows live logs from the systemd user service

# Check if running as root
if [[ $EUID -eq 0 ]]; then
    echo "Do NOT run this script as root or with sudo!"
    exit 1
fi

echo "AutoWhisper Daemon Logs (Ctrl+C to exit)"
echo "=========================================="
echo ""

journalctl --user -u autowhisper -f

#!/bin/bash
# AutoWhisper Daemon Stop Script
# Stops the systemd user service

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check if running as root
if [[ $EUID -eq 0 ]]; then
    error "Do NOT run this script as root or with sudo!"
    exit 1
fi

info "Stopping AutoWhisper daemon..."

systemctl --user stop autowhisper.service 2>/dev/null || true

info "Daemon stopped."

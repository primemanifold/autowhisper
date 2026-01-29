#!/bin/bash
# AutoWhisper Config Apply Script
# Copies config to install directory and restarts daemon only if needed

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }
debug() { echo -e "${BLUE}[DEBUG]${NC} $1"; }

INSTALL_DIR="${AUTOWHISPER_DIR:-/opt/autowhisper}"
SOURCE_CONFIG="${1:-./config.toml}"
INSTALLED_CONFIG="$INSTALL_DIR/config.toml"

# Check if running as root (should NOT be for daemon commands)
if [[ $EUID -eq 0 ]]; then
    error "Do NOT run this script as root or with sudo!"
    exit 1
fi

# Check source config exists
if [[ ! -f "$SOURCE_CONFIG" ]]; then
    error "Source config not found: $SOURCE_CONFIG"
    error "Usage: $0 [config_file]"
    exit 1
fi

# Check installed config exists
if [[ ! -f "$INSTALLED_CONFIG" ]]; then
    error "Installed config not found: $INSTALLED_CONFIG"
    error "Please run the installer first."
    exit 1
fi

info "Comparing configurations..."

# ALL settings require restart - config is loaded once at startup and cached
# There are no dynamically-read settings in the current implementation

# Check if files are different
if ! diff -q "$SOURCE_CONFIG" "$INSTALLED_CONFIG" > /dev/null 2>&1; then
    config_changed=true
    # Show what changed
    echo ""
    info "Changes detected:"
    diff "$INSTALLED_CONFIG" "$SOURCE_CONFIG" | grep -E "^[<>]" | head -20 || true
    echo ""
else
    config_changed=false
fi

# Apply the config
if $config_changed; then
    info "Configuration has changed, applying..."
    
    # Backup old config
    cp "$INSTALLED_CONFIG" "$INSTALLED_CONFIG.bak"
    debug "Backed up old config to $INSTALLED_CONFIG.bak"
    
    # Copy new config (need sudo for /opt)
    if [[ -w "$INSTALLED_CONFIG" ]]; then
        cp "$SOURCE_CONFIG" "$INSTALLED_CONFIG"
    else
        info "Elevating privileges to copy config..."
        sudo cp "$SOURCE_CONFIG" "$INSTALLED_CONFIG"
        sudo chown $USER:$USER "$INSTALLED_CONFIG"
    fi
    
    info "Config applied to $INSTALLED_CONFIG"
    
    # All config changes require restart (config is loaded once at startup)
    # Check if daemon is running
    if systemctl --user is-active --quiet autowhisper 2>/dev/null; then
        info "Restarting daemon to apply changes..."
        systemctl --user restart autowhisper
        sleep 1
        
        if systemctl --user is-active --quiet autowhisper 2>/dev/null; then
            info "Daemon restarted successfully!"
        else
            error "Daemon failed to restart. Check logs:"
            error "  journalctl --user -u autowhisper -n 20"
        fi
    else
        warn "Daemon is not running. Start it with:"
        warn "  systemctl --user start autowhisper"
    fi
else
    info "No changes detected. Config is already up to date."
fi

echo ""
info "Done!"

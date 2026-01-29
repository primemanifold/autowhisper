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

# Settings that require a restart when changed
# These are loaded at startup and cached
RESTART_REQUIRED_PATTERNS=(
    # Model settings (loaded once at startup)
    "^size\\s*="
    "^device\\s*="
    "^compute_type\\s*="
    "^beam_size\\s*="
    "^language\\s*="
    "^num_threads\\s*="
    # Audio settings (affect stream initialization)
    "^sample_rate\\s*="
    "^channels\\s*="
    "^buffer_size\\s*="
    "^vad_enabled\\s*="
    "^vad_threshold\\s*="
    "^silence_duration\\s*="
    "^max_duration\\s*="
    # Hotkey settings (registered at startup)
    "^mode\\s*="
    "^trigger\\s*="
    "^cancel\\s*="
    # Daemon settings
    "^log_level\\s*="
    "^work_dir\\s*="
)

# Settings that DON'T require restart (used dynamically)
# - output.method, output.auto_paste, etc.
# - feedback.enabled, feedback.frequency_*, feedback.duration, feedback.volume

# Function to extract value for a key from config
get_config_value() {
    local file="$1"
    local pattern="$2"
    grep -E "$pattern" "$file" 2>/dev/null | head -1 || echo ""
}

# Check if any restart-required setting changed
needs_restart=false
changed_settings=()

for pattern in "${RESTART_REQUIRED_PATTERNS[@]}"; do
    old_value=$(get_config_value "$INSTALLED_CONFIG" "$pattern")
    new_value=$(get_config_value "$SOURCE_CONFIG" "$pattern")
    
    if [[ "$old_value" != "$new_value" && -n "$new_value" ]]; then
        needs_restart=true
        # Extract setting name for display
        setting_name=$(echo "$pattern" | sed 's/\^//; s/\\s\*=//; s/\\//g')
        changed_settings+=("$setting_name: '$old_value' -> '$new_value'")
    fi
done

# Check if files are different at all (catch any other changes)
if ! diff -q "$SOURCE_CONFIG" "$INSTALLED_CONFIG" > /dev/null 2>&1; then
    config_changed=true
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
    
    if $needs_restart; then
        echo ""
        warn "The following settings changed and require a restart:"
        for change in "${changed_settings[@]}"; do
            echo "  - $change"
        done
        echo ""
        
        # Check if daemon is running
        if systemctl --user is-active --quiet autowhisper 2>/dev/null; then
            info "Restarting daemon..."
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
        info "Changes don't require a restart (output/feedback settings only)."
        info "The daemon will use new settings on next transcription."
    fi
else
    info "No changes detected. Config is already up to date."
fi

echo ""
info "Done!"

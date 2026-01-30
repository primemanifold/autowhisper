#!/bin/bash

# AutoWhisper System Compatibility Check
# Run this before installing to diagnose potential issues

# Don't use set -e because ((var++)) returns 1 when var is 0, causing false exits

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
PASS=0
WARN=0
FAIL=0

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_pass() {
    echo -e "  ${GREEN}[PASS]${NC} $1"
    ((PASS++))
}

print_warn() {
    echo -e "  ${YELLOW}[WARN]${NC} $1"
    ((WARN++))
}

print_fail() {
    echo -e "  ${RED}[FAIL]${NC} $1"
    ((FAIL++))
}

print_info() {
    echo -e "  ${BLUE}[INFO]${NC} $1"
}

print_fix() {
    echo -e "         ${YELLOW}FIX:${NC} $1"
}

echo ""
echo -e "${BLUE}AutoWhisper System Compatibility Check${NC}"
echo "========================================"
echo ""

# ------------------------------------------------------------------------------
# Check 1: Operating System
# ------------------------------------------------------------------------------
print_header "Operating System"

if [ -f /etc/os-release ]; then
    . /etc/os-release
    print_info "OS: $PRETTY_NAME"

    if [[ "$ID" == "ubuntu" ]]; then
        VERSION_NUM=$(echo "$VERSION_ID" | cut -d. -f1)
        if [[ "$VERSION_NUM" -ge 22 ]]; then
            print_pass "Ubuntu $VERSION_ID is supported"
        else
            print_warn "Ubuntu $VERSION_ID is old, recommend 22.04 or newer"
        fi
    elif [[ "$ID" == "debian" || "$ID" == "pop" || "$ID" == "linuxmint" ]]; then
        print_pass "$NAME is likely compatible (Debian-based)"
    else
        print_warn "$NAME may work but is not officially tested"
    fi
else
    print_warn "Could not detect OS"
fi

# Check for X11 (required for hotkeys and text injection)
if [ -n "$DISPLAY" ]; then
    print_pass "X11 display detected: $DISPLAY"
else
    print_warn "No X11 display detected (required for hotkeys)"
    print_fix "Run from a graphical session, not SSH"
fi

# ------------------------------------------------------------------------------
# Check 2: CPU
# ------------------------------------------------------------------------------
print_header "CPU"

CPU_MODEL=$(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
CPU_CORES=$(nproc)

print_info "Model: $CPU_MODEL"
print_info "Cores: $CPU_CORES"

if [[ "$CPU_CORES" -ge 4 ]]; then
    print_pass "CPU cores ($CPU_CORES) meets requirement (4+)"
else
    print_warn "Only $CPU_CORES cores detected, may be slow"
fi

# ------------------------------------------------------------------------------
# Check 3: RAM
# ------------------------------------------------------------------------------
print_header "Memory"

TOTAL_RAM_KB=$(grep MemTotal /proc/meminfo | awk '{print $2}')
AVAIL_RAM_KB=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
TOTAL_RAM_GB=$((TOTAL_RAM_KB / 1024 / 1024))
AVAIL_RAM_GB=$((AVAIL_RAM_KB / 1024 / 1024))
AVAIL_RAM_MB=$((AVAIL_RAM_KB / 1024))

print_info "Total RAM: ${TOTAL_RAM_GB}GB"
print_info "Available: ${AVAIL_RAM_MB}MB"

if [[ "$TOTAL_RAM_GB" -ge 8 ]]; then
    print_pass "Total RAM (${TOTAL_RAM_GB}GB) meets requirement (8GB+)"
else
    print_warn "Total RAM (${TOTAL_RAM_GB}GB) is below recommended 8GB"
fi

if [[ "$AVAIL_RAM_MB" -ge 4000 ]]; then
    print_pass "Available RAM (${AVAIL_RAM_MB}MB) is sufficient"
elif [[ "$AVAIL_RAM_MB" -ge 2000 ]]; then
    print_warn "Available RAM (${AVAIL_RAM_MB}MB) is low, close some applications"
else
    print_fail "Available RAM (${AVAIL_RAM_MB}MB) is critically low"
    print_fix "Close applications or add more RAM"
fi

# Check swap
SWAP_FREE_KB=$(grep SwapFree /proc/meminfo | awk '{print $2}')
SWAP_TOTAL_KB=$(grep SwapTotal /proc/meminfo | awk '{print $2}')
if [[ "$SWAP_TOTAL_KB" -gt 0 && "$SWAP_FREE_KB" -lt 100000 ]]; then
    print_warn "Swap is nearly exhausted - system may be under memory pressure"
fi

# ------------------------------------------------------------------------------
# Check 4: NVIDIA GPU Detection
# ------------------------------------------------------------------------------
print_header "NVIDIA GPU"

# Check if NVIDIA hardware exists
if lspci 2>/dev/null | grep -qi nvidia; then
    GPU_INFO=$(lspci | grep -i nvidia | head -1)
    print_info "Hardware: $GPU_INFO"
    print_pass "NVIDIA GPU hardware detected"
else
    print_fail "No NVIDIA GPU hardware detected"
    print_fix "AutoWhisper requires an NVIDIA GPU for GPU acceleration"
    print_fix "CPU-only mode is possible but significantly slower"
fi

# ------------------------------------------------------------------------------
# Check 5: NVIDIA Driver
# ------------------------------------------------------------------------------
print_header "NVIDIA Driver"

if command -v nvidia-smi &> /dev/null; then
    print_pass "nvidia-smi command found"

    # Try to run nvidia-smi
    if nvidia-smi &> /dev/null; then
        DRIVER_VERSION=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1)
        CUDA_VERSION=$(nvidia-smi 2>/dev/null | grep "CUDA Version" | sed 's/.*CUDA Version: \([0-9.]*\).*/\1/')
        GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
        GPU_MEMORY=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader 2>/dev/null | head -1)

        print_pass "nvidia-smi working correctly"
        print_info "GPU: $GPU_NAME"
        print_info "VRAM: $GPU_MEMORY"
        print_info "Driver: $DRIVER_VERSION"
        print_info "CUDA: $CUDA_VERSION"

        # Check driver version
        DRIVER_MAJOR=$(echo "$DRIVER_VERSION" | cut -d. -f1)
        if [[ "$DRIVER_MAJOR" -ge 525 ]]; then
            print_pass "Driver version $DRIVER_VERSION is compatible"
        elif [[ "$DRIVER_MAJOR" -ge 470 ]]; then
            print_warn "Driver version $DRIVER_VERSION is old, recommend 525+"
            print_fix "sudo ubuntu-drivers autoinstall && sudo reboot"
        else
            print_fail "Driver version $DRIVER_VERSION is too old"
            print_fix "sudo ubuntu-drivers autoinstall && sudo reboot"
        fi

        # Check VRAM
        VRAM_MB=$(echo "$GPU_MEMORY" | grep -oE '[0-9]+')
        if [[ "$VRAM_MB" -ge 4000 ]]; then
            print_pass "VRAM (${VRAM_MB}MB) is sufficient for all models"
        elif [[ "$VRAM_MB" -ge 2000 ]]; then
            print_warn "VRAM (${VRAM_MB}MB) - use 'small' or 'base' model"
        else
            print_fail "VRAM (${VRAM_MB}MB) is low - use 'tiny' model only"
        fi

    else
        # nvidia-smi exists but fails
        print_fail "nvidia-smi failed to run"

        # Check for driver mismatch
        if nvidia-smi 2>&1 | grep -q "Driver/library version mismatch"; then
            print_fail "NVIDIA driver/library version mismatch detected!"
            print_info "This usually happens after a driver update without rebooting"
            print_fix "Try: sudo reboot"
            print_fix "If reboot doesn't help: sudo ./scripts/fix-nvidia.sh"

            # Show version details
            NVML_VER=$(nvidia-smi 2>&1 | grep "NVML library version" | awk '{print $NF}')
            KERNEL_VER=$(modinfo nvidia 2>/dev/null | grep "^version:" | awk '{print $2}')
            if [ -n "$NVML_VER" ]; then
                print_info "NVML library version: $NVML_VER"
            fi
            if [ -n "$KERNEL_VER" ]; then
                print_info "Kernel module version: $KERNEL_VER"
            fi
        else
            ERROR_MSG=$(nvidia-smi 2>&1 | head -3)
            print_info "Error: $ERROR_MSG"
            print_fix "Try: sudo ubuntu-drivers autoinstall && sudo reboot"
        fi
    fi
else
    print_fail "nvidia-smi not found - NVIDIA driver not installed"
    print_fix "Install driver: sudo ubuntu-drivers autoinstall && sudo reboot"
fi

# ------------------------------------------------------------------------------
# Check 6: CUDA Toolkit (optional but good to know)
# ------------------------------------------------------------------------------
print_header "CUDA Toolkit (Optional)"

if command -v nvcc &> /dev/null; then
    NVCC_VERSION=$(nvcc --version | grep "release" | awk '{print $5}' | tr -d ',')
    print_info "nvcc version: $NVCC_VERSION"

    # Check if it's too old
    CUDA_MAJOR=$(echo "$NVCC_VERSION" | cut -d. -f1)
    if [[ "$CUDA_MAJOR" -ge 12 ]]; then
        print_pass "CUDA Toolkit $NVCC_VERSION is current"
    elif [[ "$CUDA_MAJOR" -ge 11 ]]; then
        print_info "CUDA Toolkit $NVCC_VERSION - OK (PyTorch bundles its own CUDA)"
    else
        print_warn "CUDA Toolkit $NVCC_VERSION is very old"
    fi
else
    print_info "nvcc not found (not required - PyTorch bundles CUDA runtime)"
fi

# ------------------------------------------------------------------------------
# Check 7: Required System Packages
# ------------------------------------------------------------------------------
print_header "System Packages"

REQUIRED_PACKAGES=(
    "python3:Python 3"
    "python3-venv:Python venv"
    "xdotool:Text injection"
    "xclip:Clipboard access"
)

OPTIONAL_PACKAGES=(
    "pulseaudio:Audio system"
    "portaudio19-dev:Audio capture"
)

for pkg_info in "${REQUIRED_PACKAGES[@]}"; do
    pkg=$(echo "$pkg_info" | cut -d: -f1)
    desc=$(echo "$pkg_info" | cut -d: -f2)
    if dpkg -l "$pkg" &> /dev/null 2>&1 || command -v "$pkg" &> /dev/null; then
        print_pass "$desc ($pkg) installed"
    else
        print_fail "$desc ($pkg) not installed"
        print_fix "sudo apt install $pkg"
    fi
done

for pkg_info in "${OPTIONAL_PACKAGES[@]}"; do
    pkg=$(echo "$pkg_info" | cut -d: -f1)
    desc=$(echo "$pkg_info" | cut -d: -f2)
    if dpkg -l "$pkg" &> /dev/null 2>&1; then
        print_pass "$desc ($pkg) installed"
    else
        print_warn "$desc ($pkg) not installed (will be installed by installer)"
    fi
done

# Check Python version
if command -v python3 &> /dev/null; then
    PY_VERSION=$(python3 --version | awk '{print $2}')
    PY_MAJOR=$(echo "$PY_VERSION" | cut -d. -f1)
    PY_MINOR=$(echo "$PY_VERSION" | cut -d. -f2)
    print_info "Python version: $PY_VERSION"

    if [[ "$PY_MAJOR" -eq 3 && "$PY_MINOR" -ge 10 ]]; then
        print_pass "Python $PY_VERSION meets requirement (3.10+)"
    elif [[ "$PY_MAJOR" -eq 3 && "$PY_MINOR" -ge 8 ]]; then
        print_warn "Python $PY_VERSION may work but 3.10+ recommended"
    else
        print_fail "Python $PY_VERSION is too old, need 3.10+"
        print_fix "sudo apt install python3.10"
    fi
fi

# ------------------------------------------------------------------------------
# Check 8: Audio
# ------------------------------------------------------------------------------
print_header "Audio System"

# Check for audio devices (with timeout to avoid hanging)
if command -v arecord &> /dev/null; then
    MIC_COUNT=$(timeout 5 arecord -l 2>/dev/null | grep -c "card" || echo "0")
    if [[ "$MIC_COUNT" -gt 0 ]]; then
        print_pass "Found $MIC_COUNT audio capture device(s)"
    else
        print_warn "No audio capture devices found"
        print_fix "Check microphone connection and PulseAudio settings"
    fi
else
    print_warn "arecord not found - cannot check audio devices"
fi

# Check PulseAudio/PipeWire (with timeout)
if command -v pactl &> /dev/null; then
    if timeout 5 pactl info &> /dev/null; then
        AUDIO_SERVER=$(timeout 5 pactl info 2>/dev/null | grep "Server Name" | cut -d: -f2 | xargs || echo "unknown")
        print_pass "Audio server running: $AUDIO_SERVER"
    else
        print_warn "PulseAudio/PipeWire not responding (may work in graphical session)"
    fi
else
    print_info "pactl not found - audio check skipped"
fi

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
print_header "Summary"

echo ""
echo -e "  ${GREEN}Passed:${NC}  $PASS"
echo -e "  ${YELLOW}Warnings:${NC} $WARN"
echo -e "  ${RED}Failed:${NC}  $FAIL"
echo ""

if [[ "$FAIL" -eq 0 ]]; then
    if [[ "$WARN" -eq 0 ]]; then
        echo -e "${GREEN}System is fully compatible with AutoWhisper!${NC}"
        echo ""
        echo "You can proceed with installation:"
        echo "  sudo ./install.sh"
    else
        echo -e "${YELLOW}System is compatible with some warnings.${NC}"
        echo ""
        echo "You can proceed with installation, but review warnings above:"
        echo "  sudo ./install.sh"
    fi
else
    echo -e "${RED}System has issues that need to be fixed before installation.${NC}"
    echo ""
    echo "Please fix the FAIL items above, then run this check again."
    echo ""

    # Provide specific guidance based on common failures
    if nvidia-smi 2>&1 | grep -q "Driver/library version mismatch" 2>/dev/null; then
        echo "Most likely fix: Reboot your computer"
        echo "  sudo reboot"
        echo ""
        echo "If reboot doesn't help, run the fix script:"
        echo "  sudo ./scripts/fix-nvidia.sh"
    elif ! command -v nvidia-smi &> /dev/null; then
        echo "Install NVIDIA drivers:"
        echo "  sudo ubuntu-drivers autoinstall"
        echo "  sudo reboot"
    fi
fi

echo ""
exit $FAIL

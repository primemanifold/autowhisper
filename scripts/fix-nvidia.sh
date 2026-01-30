#!/bin/bash

# AutoWhisper NVIDIA Driver Fix Script
# Diagnoses and fixes common NVIDIA driver issues

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

confirm() {
    echo ""
    read -p "$1 [y/N] " response
    case "$response" in
        [yY][eE][sS]|[yY]) return 0 ;;
        *) return 1 ;;
    esac
}

echo ""
echo -e "${BLUE}AutoWhisper NVIDIA Driver Fix Script${NC}"
echo "======================================"
echo ""

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root (use sudo)${NC}"
    echo "Usage: sudo $0"
    exit 1
fi

# Get actual user for later
ACTUAL_USER=${SUDO_USER:-$USER}

# ------------------------------------------------------------------------------
# Step 1: Detect the problem
# ------------------------------------------------------------------------------
print_header "Diagnosing NVIDIA Issues"

PROBLEM=""
NVIDIA_HW_EXISTS=false
DRIVER_INSTALLED=false
DRIVER_WORKING=false
DRIVER_VERSION=""
KERNEL_MODULE_VERSION=""
NVML_VERSION=""

# Check for NVIDIA hardware
if lspci 2>/dev/null | grep -qi nvidia; then
    NVIDIA_HW_EXISTS=true
    GPU_INFO=$(lspci | grep -i nvidia | grep -iE "vga|3d" | head -1)
    print_success "NVIDIA GPU detected: $GPU_INFO"
else
    print_error "No NVIDIA GPU hardware found"
    echo ""
    echo "AutoWhisper requires an NVIDIA GPU for GPU acceleration."
    echo "Your system does not appear to have an NVIDIA GPU."
    echo ""
    echo "Options:"
    echo "  1. Use CPU-only mode (slower but functional)"
    echo "  2. Add an NVIDIA GPU to your system"
    exit 1
fi

# Check if driver is installed
if command -v nvidia-smi &> /dev/null; then
    DRIVER_INSTALLED=true
    print_success "nvidia-smi command found"
else
    PROBLEM="no_driver"
    print_error "NVIDIA driver not installed"
fi

# If driver is installed, check if it works
if [ "$DRIVER_INSTALLED" = true ]; then
    if nvidia-smi &> /dev/null; then
        DRIVER_WORKING=true
        DRIVER_VERSION=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1)
        print_success "Driver is working (version $DRIVER_VERSION)"
        echo ""
        echo -e "${GREEN}Your NVIDIA driver appears to be working correctly!${NC}"
        echo ""
        echo "If you're still having issues with AutoWhisper, try:"
        echo "  1. Reboot your computer"
        echo "  2. Re-run ./scripts/check-system.sh"
        echo "  3. Check AutoWhisper logs: journalctl --user -u autowhisper -f"
        exit 0
    else
        # Driver installed but not working - diagnose why
        ERROR_OUTPUT=$(nvidia-smi 2>&1)

        if echo "$ERROR_OUTPUT" | grep -q "Driver/library version mismatch"; then
            PROBLEM="version_mismatch"
            NVML_VERSION=$(echo "$ERROR_OUTPUT" | grep "NVML library version" | awk '{print $NF}')
            KERNEL_MODULE_VERSION=$(modinfo nvidia 2>/dev/null | grep "^version:" | awk '{print $2}')
            print_error "Driver/library version mismatch!"
            print_info "NVML library: $NVML_VERSION"
            print_info "Kernel module: $KERNEL_MODULE_VERSION"

        elif echo "$ERROR_OUTPUT" | grep -q "GPU has fallen off the bus"; then
            PROBLEM="gpu_fallen_off"
            print_error "GPU has fallen off the bus (hardware issue)"

        elif echo "$ERROR_OUTPUT" | grep -q "no devices were found"; then
            PROBLEM="no_devices"
            print_error "Driver loaded but no devices found"

        else
            PROBLEM="unknown"
            print_error "Unknown driver issue:"
            echo "$ERROR_OUTPUT" | head -5
        fi
    fi
fi

# ------------------------------------------------------------------------------
# Step 2: Offer appropriate fix
# ------------------------------------------------------------------------------
print_header "Recommended Fix"

case "$PROBLEM" in
    "version_mismatch")
        echo "The NVIDIA driver was recently updated, but the old kernel module"
        echo "is still loaded. This is the most common issue after system updates."
        echo ""
        echo -e "${YELLOW}Solution 1: Reboot (Recommended)${NC}"
        echo "  A simple reboot will load the correct kernel module."
        echo ""
        echo -e "${YELLOW}Solution 2: Reload kernel modules (Advanced)${NC}"
        echo "  Unload and reload NVIDIA kernel modules without rebooting."
        echo "  This may fail if any process is using the GPU."
        echo ""

        if confirm "Would you like to try reloading kernel modules? (requires no GPU processes)"; then
            print_header "Attempting Kernel Module Reload"

            # Check for GPU processes
            GPU_PROCS=$(lsof /dev/nvidia* 2>/dev/null | tail -n +2 || true)
            if [ -n "$GPU_PROCS" ]; then
                print_warn "The following processes are using the GPU:"
                echo "$GPU_PROCS"
                echo ""
                if confirm "Kill these processes and continue?"; then
                    # Try to kill GPU processes
                    for pid in $(lsof -t /dev/nvidia* 2>/dev/null || true); do
                        print_info "Killing process $pid..."
                        kill -9 "$pid" 2>/dev/null || true
                    done
                    sleep 2
                else
                    echo ""
                    echo "Please close GPU-using applications and try again, or reboot:"
                    echo "  sudo reboot"
                    exit 1
                fi
            fi

            # Unload modules
            print_info "Unloading NVIDIA kernel modules..."
            rmmod nvidia_uvm 2>/dev/null || true
            rmmod nvidia_drm 2>/dev/null || true
            rmmod nvidia_modeset 2>/dev/null || true
            rmmod nvidia 2>/dev/null || true

            # Reload modules
            print_info "Reloading NVIDIA kernel modules..."
            modprobe nvidia
            modprobe nvidia_uvm
            modprobe nvidia_drm
            modprobe nvidia_modeset

            sleep 2

            # Test
            if nvidia-smi &> /dev/null; then
                print_success "Driver is now working!"
                nvidia-smi
            else
                print_error "Module reload failed. A reboot is required."
                echo ""
                echo "Run: sudo reboot"
            fi
        else
            echo ""
            echo "Please reboot to fix the driver mismatch:"
            echo "  sudo reboot"
        fi
        ;;

    "no_driver")
        echo "No NVIDIA driver is currently installed."
        echo ""
        echo "Ubuntu can automatically detect and install the best driver."
        echo ""

        if confirm "Install NVIDIA driver automatically using ubuntu-drivers?"; then
            print_header "Installing NVIDIA Driver"

            print_info "Updating package list..."
            apt-get update

            print_info "Detecting recommended driver..."
            RECOMMENDED=$(ubuntu-drivers devices 2>/dev/null | grep "recommended" | awk '{print $3}' || true)

            if [ -n "$RECOMMENDED" ]; then
                print_info "Recommended driver: $RECOMMENDED"
            fi

            print_info "Installing drivers (this may take a few minutes)..."
            ubuntu-drivers autoinstall

            print_success "Driver installation complete!"
            echo ""
            echo -e "${YELLOW}You must reboot for the driver to take effect:${NC}"
            echo "  sudo reboot"

            if confirm "Reboot now?"; then
                reboot
            fi
        else
            echo ""
            echo "To install manually:"
            echo "  sudo ubuntu-drivers autoinstall"
            echo "  sudo reboot"
        fi
        ;;

    "gpu_fallen_off")
        echo "The GPU has 'fallen off the bus' - this is usually a hardware issue."
        echo ""
        echo "Possible causes:"
        echo "  - Overheating (check GPU temperature and fans)"
        echo "  - Power supply issues"
        echo "  - Loose PCIe connection"
        echo "  - Hardware failure"
        echo ""
        echo "Try these steps:"
        echo "  1. Shut down completely (not restart)"
        echo "  2. Wait 30 seconds"
        echo "  3. Power on"
        echo ""
        echo "If the issue persists, check:"
        echo "  - GPU temperature with 'sensors' command"
        echo "  - System logs: journalctl -b | grep -i nvidia"
        echo "  - Physical GPU seating (desktop) or consider RMA (laptop)"
        ;;

    "no_devices")
        echo "The driver is loaded but can't find the GPU."
        echo ""
        echo "This can happen if:"
        echo "  - The kernel module doesn't match the hardware"
        echo "  - Secure Boot is blocking the driver"
        echo "  - The GPU is disabled in BIOS"
        echo ""

        # Check Secure Boot
        if command -v mokutil &> /dev/null; then
            SB_STATE=$(mokutil --sb-state 2>/dev/null || echo "unknown")
            print_info "Secure Boot: $SB_STATE"

            if echo "$SB_STATE" | grep -qi "enabled"; then
                print_warn "Secure Boot is enabled"
                echo ""
                echo "NVIDIA drivers need to be signed for Secure Boot."
                echo "The driver may have been installed without proper signing."
                echo ""
                echo "Options:"
                echo "  1. Reinstall driver with DKMS signing (ubuntu-drivers autoinstall)"
                echo "  2. Disable Secure Boot in BIOS"
            fi
        fi

        if confirm "Reinstall NVIDIA driver?"; then
            print_header "Reinstalling NVIDIA Driver"

            print_info "Removing existing NVIDIA packages..."
            apt-get remove --purge -y 'nvidia-*' 2>/dev/null || true
            apt-get autoremove -y

            print_info "Installing fresh driver..."
            apt-get update
            ubuntu-drivers autoinstall

            print_success "Reinstallation complete!"
            echo ""
            echo "Please reboot: sudo reboot"
        fi
        ;;

    "unknown")
        echo "An unknown issue is preventing the NVIDIA driver from working."
        echo ""
        echo "Collecting diagnostic information..."
        echo ""

        echo "Kernel version: $(uname -r)"
        echo "NVIDIA packages installed:"
        dpkg -l | grep -i nvidia | awk '{print "  " $2 " " $3}'
        echo ""
        echo "Kernel modules:"
        lsmod | grep nvidia || echo "  No NVIDIA modules loaded"
        echo ""
        echo "Recent NVIDIA-related kernel messages:"
        dmesg | grep -i nvidia | tail -10 || echo "  None found"
        echo ""

        echo "Try reinstalling the driver:"
        echo "  sudo apt remove --purge 'nvidia-*'"
        echo "  sudo apt autoremove"
        echo "  sudo ubuntu-drivers autoinstall"
        echo "  sudo reboot"
        ;;
esac

echo ""
print_header "Next Steps"
echo ""
echo "After fixing the driver issue:"
echo ""
echo "  1. Run the system check again:"
echo "     ./scripts/check-system.sh"
echo ""
echo "  2. If all checks pass, install AutoWhisper:"
echo "     sudo ./install.sh"
echo ""

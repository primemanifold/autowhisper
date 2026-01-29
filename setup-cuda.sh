#!/bin/bash
# CUDA Setup Script for AutoWhisper
# Detects GPU, checks CUDA toolkit compatibility, and installs correct version

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Detect Ubuntu version
get_ubuntu_version() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "${VERSION_ID}"
    else
        echo ""
    fi
}

# Get GPU compute capability from nvidia-smi
get_gpu_compute_capability() {
    if ! command -v nvidia-smi &> /dev/null; then
        echo ""
        return
    fi
    
    local gpu_name
    gpu_name=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
    
    if [ -z "$gpu_name" ]; then
        echo ""
        return
    fi
    
    # Map GPU names to compute capabilities
    case "$gpu_name" in
        *"5090"*|*"5080"*|*"5070"*|*"5060"*|*"5050"*)
            echo "120"  # Blackwell
            ;;
        *"4090"*|*"4080"*|*"4070"*|*"4060"*|*"4050"*)
            echo "89"   # Ada Lovelace
            ;;
        *"3090"*|*"3080"*|*"3070"*|*"3060"*|*"3050"*|*"A100"*|*"A6000"*|*"A5000"*|*"A4000"*)
            echo "86"   # Ampere
            ;;
        *"2080"*|*"2070"*|*"2060"*|*"T4"*|*"Quadro RTX"*)
            echo "75"   # Turing
            ;;
        *"1080"*|*"1070"*|*"1060"*|*"1050"*|*"P100"*|*"P40"*|*"P6000"*)
            echo "61"   # Pascal
            ;;
        *"V100"*)
            echo "70"   # Volta
            ;;
        *)
            # Try to detect via CUDA if available
            echo "unknown"
            ;;
    esac
}

# Get minimum CUDA version required for compute capability
get_required_cuda_version() {
    local compute_cap=$1
    case "$compute_cap" in
        "120") echo "13.0" ;;  # Blackwell requires CUDA 13.0+
        "89")  echo "11.8" ;;  # Ada Lovelace
        "86")  echo "11.1" ;;  # Ampere
        "75")  echo "10.0" ;;  # Turing
        "70")  echo "9.0"  ;;  # Volta
        "61")  echo "8.0"  ;;  # Pascal
        *)     echo "11.0" ;;  # Default
    esac
}

# Get installed CUDA toolkit version
get_installed_cuda_version() {
    if command -v nvcc &> /dev/null; then
        nvcc --version 2>/dev/null | grep "release" | sed 's/.*release \([0-9]*\.[0-9]*\).*/\1/'
    else
        echo ""
    fi
}

# Get driver's maximum supported CUDA version
get_driver_cuda_version() {
    if command -v nvidia-smi &> /dev/null; then
        nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1
        # Actually get CUDA version from nvidia-smi
        nvidia-smi 2>/dev/null | grep "CUDA Version" | sed 's/.*CUDA Version: \([0-9]*\.[0-9]*\).*/\1/'
    else
        echo ""
    fi
}

# Compare versions (returns 0 if $1 >= $2)
version_gte() {
    [ "$(printf '%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]
}

# Install CUDA from NVIDIA repository
install_cuda_from_nvidia() {
    local cuda_version=$1
    local ubuntu_version=$2
    
    # Map Ubuntu version to NVIDIA repo name
    local repo_version
    case "$ubuntu_version" in
        "24.04") repo_version="ubuntu2404" ;;
        "22.04") repo_version="ubuntu2204" ;;
        "20.04") repo_version="ubuntu2004" ;;
        *)
            error "Unsupported Ubuntu version: $ubuntu_version"
            return 1
            ;;
    esac
    
    # Determine CUDA package version
    local cuda_major cuda_minor
    cuda_major=$(echo "$cuda_version" | cut -d. -f1)
    cuda_minor=$(echo "$cuda_version" | cut -d. -f2)
    
    info "Installing CUDA ${cuda_major}.${cuda_minor} from NVIDIA repository..."
    
    # Download and install keyring
    local keyring_url="https://developer.download.nvidia.com/compute/cuda/repos/${repo_version}/x86_64/cuda-keyring_1.1-1_all.deb"
    local keyring_file="/tmp/cuda-keyring.deb"
    
    info "Downloading CUDA repository keyring..."
    if ! wget -q "$keyring_url" -O "$keyring_file"; then
        error "Failed to download CUDA keyring"
        return 1
    fi
    
    info "Installing keyring..."
    sudo dpkg -i "$keyring_file"
    rm -f "$keyring_file"
    
    info "Updating package lists..."
    sudo apt-get update
    
    # Remove old Ubuntu CUDA toolkit if present
    if dpkg -l | grep -q "nvidia-cuda-toolkit"; then
        warn "Removing old Ubuntu CUDA toolkit..."
        sudo apt-get remove -y nvidia-cuda-toolkit nvidia-cuda-toolkit-doc 2>/dev/null || true
    fi
    
    # Install CUDA toolkit
    local cuda_package="cuda-toolkit-${cuda_major}-${cuda_minor}"
    info "Installing ${cuda_package}..."
    
    if ! sudo apt-get install -y "$cuda_package"; then
        # Try without minor version
        cuda_package="cuda-toolkit-${cuda_major}"
        warn "Trying ${cuda_package} instead..."
        sudo apt-get install -y "$cuda_package"
    fi
    
    # Set up environment
    local cuda_path="/usr/local/cuda-${cuda_major}.${cuda_minor}"
    if [ ! -d "$cuda_path" ]; then
        cuda_path="/usr/local/cuda"
    fi
    
    info "Setting up environment..."
    
    # Add to current session
    export PATH="${cuda_path}/bin:$PATH"
    export LD_LIBRARY_PATH="${cuda_path}/lib64:$LD_LIBRARY_PATH"
    
    # Add to bashrc if not already there
    if ! grep -q "cuda" ~/.bashrc 2>/dev/null; then
        echo "" >> ~/.bashrc
        echo "# CUDA environment" >> ~/.bashrc
        echo "export PATH=${cuda_path}/bin:\$PATH" >> ~/.bashrc
        echo "export LD_LIBRARY_PATH=${cuda_path}/lib64:\$LD_LIBRARY_PATH" >> ~/.bashrc
        info "Added CUDA to ~/.bashrc"
    fi
    
    return 0
}

# Note: whisper.cpp build function removed - project now uses faster-whisper (Python)

# Verify Python/CUDA setup for faster-whisper
verify_python_cuda() {
    info "Verifying Python CUDA setup for faster-whisper..."
    
    if ! command -v python3 &> /dev/null; then
        warn "Python 3 not found. Please install Python 3."
        return 1
    fi
    
    # Check if PyTorch with CUDA is available
    if python3 -c "import torch; exit(0 if torch.cuda.is_available() else 1)" 2>/dev/null; then
        info "PyTorch with CUDA support is available"
        python3 -c "import torch; print(f'CUDA available: {torch.cuda.is_available()}'); print(f'CUDA version: {torch.version.cuda}'); print(f'GPU: {torch.cuda.get_device_name(0) if torch.cuda.is_available() else \"N/A\"}')" 2>/dev/null
        return 0
    else
        warn "PyTorch with CUDA support not found."
        warn "Install (most GPUs): pip install --upgrade torch torchaudio --index-url https://download.pytorch.org/whl/cu124"
        warn "RTX 50xx / sm_120: pip install --upgrade --pre torch torchaudio --index-url https://download.pytorch.org/whl/nightly/cu128"
        return 1
    fi
}

# Main script
main() {
    echo "=============================================="
    echo "  AutoWhisper CUDA Setup Script"
    echo "=============================================="
    echo ""
    
    # Check if running on Linux
    if [ "$(uname)" != "Linux" ]; then
        error "This script only supports Linux"
        exit 1
    fi
    
    # Get Ubuntu version
    local ubuntu_version
    ubuntu_version=$(get_ubuntu_version)
    info "Ubuntu version: ${ubuntu_version:-unknown}"
    
    # Check for NVIDIA GPU
    if ! command -v nvidia-smi &> /dev/null; then
        warn "nvidia-smi not found. No NVIDIA GPU detected or drivers not installed."
        warn "faster-whisper will use CPU (slower)."
        exit 0
    fi
    
    # Get GPU info
    local gpu_name
    gpu_name=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
    info "Detected GPU: $gpu_name"
    
    # Get compute capability
    local compute_cap
    compute_cap=$(get_gpu_compute_capability)
    info "Compute capability: ${compute_cap:-unknown}"
    
    if [ "$compute_cap" = "unknown" ] || [ -z "$compute_cap" ]; then
        warn "Could not determine GPU compute capability"
        warn "faster-whisper will use CPU (slower)"
        exit 0
    fi
    
    # Get required CUDA version
    local required_cuda
    required_cuda=$(get_required_cuda_version "$compute_cap")
    info "Required CUDA version: >= $required_cuda"
    
    # Get driver's supported CUDA version
    local driver_cuda
    driver_cuda=$(get_driver_cuda_version)
    info "Driver supports CUDA: $driver_cuda"
    
    # Get installed CUDA toolkit version
    local installed_cuda
    installed_cuda=$(get_installed_cuda_version)
    
    if [ -n "$installed_cuda" ]; then
        info "Installed CUDA toolkit: $installed_cuda"
        
        if version_gte "$installed_cuda" "$required_cuda"; then
            info "CUDA toolkit is compatible with your GPU!"
            verify_python_cuda
            echo ""
            info "CUDA setup complete! faster-whisper will use GPU acceleration."
            info "Next steps:"
            info "  1. Install Python dependencies: pip install -r requirements.txt"
            info "  2. Run autowhisper: python3 -m autowhisper"
            exit 0
        else
            warn "Installed CUDA toolkit ($installed_cuda) is too old for your GPU"
            warn "Need CUDA >= $required_cuda for compute capability $compute_cap"
        fi
    else
        warn "No CUDA toolkit installed"
    fi
    
    # Check if driver supports required CUDA version
    if [ -n "$driver_cuda" ] && ! version_gte "$driver_cuda" "$required_cuda"; then
        error "Your NVIDIA driver only supports CUDA $driver_cuda"
        error "You need a newer driver that supports CUDA >= $required_cuda"
        warn "faster-whisper will use CPU (slower)"
        exit 1
    fi
    
    # Prompt for installation
    echo ""
    echo "=============================================="
    echo "  CUDA Installation Required"
    echo "=============================================="
    echo ""
    echo "Your GPU ($gpu_name) requires CUDA >= $required_cuda"
    echo ""
    read -p "Install CUDA from NVIDIA repository? [Y/n] " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        warn "Skipping CUDA installation. faster-whisper will use CPU (slower)."
        exit 0
    fi
    
    # Determine best CUDA version to install
    local install_version
    if [ -n "$driver_cuda" ]; then
        # Install the version matching driver capability
        install_version="$driver_cuda"
    else
        # Default to required version
        install_version="$required_cuda"
    fi
    
    # Install CUDA
    if install_cuda_from_nvidia "$install_version" "$ubuntu_version"; then
        info "CUDA installation complete!"
        
        # Verify installation
        if command -v nvcc &> /dev/null; then
            local new_version
            new_version=$(nvcc --version 2>/dev/null | grep "release" | sed 's/.*release \([0-9]*\.[0-9]*\).*/\1/')
            info "Verified CUDA toolkit: $new_version"
        fi
        
        verify_python_cuda
        
        echo ""
        echo "=============================================="
        echo "  CUDA Setup Complete!"
        echo "=============================================="
        echo ""
        info "CUDA has been installed and configured."
        info "Please run: source ~/.bashrc"
        echo ""
        info "Next steps:"
        info "  1. Install Python dependencies: pip install -r requirements.txt"
        info "  2. Install PyTorch with CUDA (most GPUs): pip install --upgrade torch torchaudio --index-url https://download.pytorch.org/whl/cu124"
        info "     RTX 50xx / sm_120: pip install --upgrade --pre torch torchaudio --index-url https://download.pytorch.org/whl/nightly/cu128"
        info "  3. Run autowhisper: python3 -m autowhisper"
    else
        error "CUDA installation failed"
        warn "faster-whisper will use CPU (slower)"
        exit 1
    fi
}

main "$@"

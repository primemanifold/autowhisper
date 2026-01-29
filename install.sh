#!/bin/bash
set -e

# AutoWhisper Installation Script
# Python/faster-whisper version for Ubuntu 24.04 with NVIDIA GPU

# Capture script directory BEFORE any cd commands
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================="
echo "AutoWhisper Installation Script"
echo "Python/faster-whisper edition"
echo "=========================================="
echo ""

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (use sudo)"
   exit 1
fi

# Get the actual user (not root)
ACTUAL_USER=${SUDO_USER:-$USER}
ACTUAL_HOME=$(eval echo ~$ACTUAL_USER)
ACTUAL_UID=$(id -u $ACTUAL_USER)

echo "Installing for user: $ACTUAL_USER"
echo "Home directory: $ACTUAL_HOME"
echo ""

# Install system dependencies
echo "[1/6] Installing system dependencies..."
apt-get update
apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev \
    build-essential \
    pkg-config \
    libssl-dev \
    libasound2-dev \
    libportaudio2 \
    portaudio19-dev \
    libx11-dev \
    libxext-dev \
    libxft-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxrender-dev \
    libxfixes-dev \
    libxi-dev \
    libxrandr-dev \
    xdotool \
    xclip \
    pulseaudio \
    nvidia-cuda-toolkit \
    git \
    curl

echo "[2/6] Checking NVIDIA GPU..."
if ! command -v nvidia-smi &> /dev/null; then
    echo "WARNING: nvidia-smi not found. Please install NVIDIA drivers."
    echo "Continue anyway? (y/n)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    nvidia-smi
    echo "GPU check passed!"
fi

echo "[3/6] Setting up installation directory..."
INSTALL_DIR="/opt/autowhisper"
mkdir -p "$INSTALL_DIR/src/autowhisper"

# Copy source files
cp "$SCRIPT_DIR/src/autowhisper/"*.py "$INSTALL_DIR/src/autowhisper/"
cp "$SCRIPT_DIR/config.toml" "$INSTALL_DIR/"
cp "$SCRIPT_DIR/requirements.txt" "$INSTALL_DIR/"
cp "$SCRIPT_DIR/pyproject.toml" "$INSTALL_DIR/"
cp "$SCRIPT_DIR/autowhisper.service" "$INSTALL_DIR/"

chown -R $ACTUAL_USER:$ACTUAL_USER "$INSTALL_DIR"

# Select PyTorch wheel index based on GPU compute capability.
# RTX 50xx (Blackwell, sm_120) requires CUDA 12.8+ wheels for native support.
PYTORCH_INDEX_URL="https://download.pytorch.org/whl/cu124"
PYTORCH_PRE_FLAG=""
if command -v nvidia-smi &> /dev/null; then
    GPU_CAP="$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader | tr -d ' ' | { read -r cap; echo "$cap"; })"
    GPU_CAP_MAJOR="${GPU_CAP%%.*}"
    if [[ -n "$GPU_CAP_MAJOR" && "$GPU_CAP_MAJOR" -ge 12 ]]; then
        PYTORCH_INDEX_URL="https://download.pytorch.org/whl/nightly/cu128"
        PYTORCH_PRE_FLAG="--pre"
    fi
fi

echo "[4/6] Creating Python virtual environment and installing CUDA PyTorch..."
cd "$INSTALL_DIR"

# Create venv as actual user
sudo -u $ACTUAL_USER python3 -m venv venv

# Activate and install dependencies with CUDA support
sudo -u $ACTUAL_USER bash -c "
    source venv/bin/activate
    pip install --upgrade pip

    # Install PyTorch + torchaudio (GPU support depends on wheel build).
    echo \"Installing PyTorch + torchaudio from: ${PYTORCH_INDEX_URL}\"
    pip install --upgrade ${PYTORCH_PRE_FLAG} torch torchaudio --index-url ${PYTORCH_INDEX_URL}

    # Install remaining dependencies
    pip install -r requirements.txt
    pip install -e .

    # Verify CUDA is available
    echo ''
    echo 'Verifying CUDA support...'
    python3 -c '
import torch
print(f\"PyTorch version: {torch.__version__}\")
print(f\"CUDA arch list: {torch.cuda.get_arch_list() if torch.cuda.is_available() else 'N/A'}\")
print(f\"CUDA available: {torch.cuda.is_available()}\")
if torch.cuda.is_available():
    print(f\"CUDA version: {torch.version.cuda}\")
    print(f\"GPU: {torch.cuda.get_device_name(0)}\")
else:
    print(\"WARNING: CUDA not available - will use CPU (slower)\")
'
"

echo "[5/6] Pre-downloading faster-whisper model..."
# Download the model and test CUDA inference
sudo -u $ACTUAL_USER bash -c "
    source venv/bin/activate
    python3 -c '
import torch
from faster_whisper import WhisperModel

print(\"Downloading distil-large-v3 model (this may take a while)...\")

# Use CUDA if available, otherwise CPU
device = \"cuda\" if torch.cuda.is_available() else \"cpu\"

# Use float16 for CUDA (compatible with all modern GPUs including RTX 50xx Blackwell)
# int8_float16 is NOT supported on newer architectures (sm_120+)
compute_type = \"float16\" if device == \"cuda\" else \"float32\"

print(f\"Loading model on {device} with {compute_type}...\")
model = WhisperModel(\"distil-large-v3\", device=device, compute_type=compute_type)
print(f\"Model loaded successfully on {device}!\")

if device == \"cuda\":
    print(f\"GPU memory used: {torch.cuda.memory_allocated() / 1024**3:.2f} GB\")

del model
'
" || echo "Model download failed - will download on first run"

echo "[6/6] Installing systemd user service..."
# Install as a user service (runs within the graphical session)
USER_SERVICE_DIR="$ACTUAL_HOME/.config/systemd/user"
mkdir -p "$USER_SERVICE_DIR"
cp "$INSTALL_DIR/autowhisper.service" "$USER_SERVICE_DIR/"
chown -R $ACTUAL_USER:$ACTUAL_USER "$ACTUAL_HOME/.config/systemd"

# Reload systemd for user (as the actual user)
sudo -u $ACTUAL_USER XDG_RUNTIME_DIR=/run/user/$ACTUAL_UID systemctl --user daemon-reload

echo ""
echo "=========================================="
echo "Installation Complete!"
echo "=========================================="
echo ""
echo "IMPORTANT: AutoWhisper runs as a user service within your graphical session."
echo "You must be logged into a graphical desktop (GNOME, KDE, etc.) for it to work."
echo ""
echo "To start AutoWhisper (run as your user, NOT with sudo):"
echo "  systemctl --user start autowhisper"
echo ""
echo "To enable on login:"
echo "  systemctl --user enable autowhisper"
echo ""
echo "To check status:"
echo "  systemctl --user status autowhisper"
echo ""
echo "To view logs:"
echo "  journalctl --user -u autowhisper -f"
echo ""
echo "Or run manually:"
echo "  cd /opt/autowhisper && source venv/bin/activate && python -m autowhisper"
echo ""
echo "Default hotkey: Shift+Super (push to talk)"
echo "Edit config: /opt/autowhisper/config.toml"
echo ""

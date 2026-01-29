#!/bin/bash
set -e

# Model download script for AutoWhisper
# Usage: sudo /usr/share/autowhisper/download-models.sh [tiny|base|small|medium]

MODEL=${1:-small}
MODELS_DIR="/usr/share/autowhisper/models"
TEMP_DIR="/tmp/whisper-model-download"

echo "Downloading Whisper $MODEL model..."

# Check if model already exists
if [ -f "$MODELS_DIR/ggml-$MODEL.bin" ]; then
    echo "Model already exists: $MODELS_DIR/ggml-$MODEL.bin"
    echo "Remove it first if you want to re-download."
    exit 0
fi

# Create temp directory
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# Download whisper.cpp if not present
if [ ! -d "whisper.cpp" ]; then
    echo "Cloning whisper.cpp..."
    git clone --depth 1 https://github.com/ggerganov/whisper.cpp.git
fi

cd whisper.cpp/models

# Download model
echo "Downloading model (this may take a few minutes)..."
bash download-ggml-model.sh "$MODEL"

# Copy to system location
echo "Installing model..."
cp "ggml-$MODEL.bin" "$MODELS_DIR/"
chmod 644 "$MODELS_DIR/ggml-$MODEL.bin"

# Update config to point to this model
CONFIG="/etc/autowhisper/config.toml"
if [ -f "$CONFIG" ]; then
    # Update model path in config
    sed -i "s|path = \".*\"|path = \"$MODELS_DIR/ggml-$MODEL.bin\"|" "$CONFIG"
    sed -i "s|size = \".*\"|size = \"$MODEL\"|" "$CONFIG"
    echo "Updated config: $CONFIG"
fi

# Cleanup
cd /
rm -rf "$TEMP_DIR"

echo ""
echo "✅ Model installed successfully!"
echo "   Location: $MODELS_DIR/ggml-$MODEL.bin"
echo ""
echo "To start using AutoWhisper:"
echo "   sudo systemctl enable --now autowhisper@\$USER"
echo ""

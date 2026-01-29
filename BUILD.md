# Build Instructions

## Prerequisites

### System Requirements

- **OS**: Ubuntu 24.04 LTS (or similar Linux with X11)
- **GPU**: NVIDIA GPU with CUDA support
- **RAM**: 4GB minimum (8GB+ recommended)
- **VRAM**: 2GB minimum for distil-large-v3 model
- **Python**: 3.10 or higher

### Required Software

```bash
# Install build tools
sudo apt update
sudo apt install -y \
    build-essential \
    pkg-config \
    libssl-dev \
    git \
    curl

# Install audio libraries
sudo apt install -y \
    libasound2-dev \
    portaudio19-dev \
    pulseaudio

# Install X11 tools for text injection
sudo apt install -y \
    xdotool \
    xclip

# Install NVIDIA CUDA Toolkit
sudo apt install -y nvidia-cuda-toolkit

# Verify CUDA installation
nvidia-smi
nvcc --version

# Install Python 3.10+
sudo apt install -y python3 python3-pip python3-venv
```

---

## Build AutoWhisper

### Manual Build

```bash
# Clone the repository
git clone <repository-url> autowhisper
cd autowhisper

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install package with dependencies
pip install -e .

# Models are downloaded automatically on first run
# Or download manually:
python -c "from faster_whisper import WhisperModel; WhisperModel('distil-large-v3')"
```

### Using the Install Script (Recommended)

```bash
cd autowhisper
sudo ./install.sh
```

This will:
1. Install all system dependencies
2. Check for NVIDIA GPU
3. Create Python virtual environment
4. Install package and dependencies
5. Download the model
6. Install systemd service

---

## Configuration

Edit `config.toml` before running:

```toml
[model]
size = "distil-large-v3"         # Model name
device = "cuda"                   # cuda or cpu
compute_type = "float16"          # float16, int8_float16, int8, float32
beam_size = 1                     # 1 = fastest
language = "en"                   # or "auto"

[hotkeys]
trigger = "shift+super"           # Your preferred hotkey
cancel = "ctrl+alt+c"

[output]
method = "inject"                 # "inject" or "clipboard"
```

---

## Running

### Foreground (for testing)

```bash
# Activate virtual environment
source venv/bin/activate

# Run directly
python -m autowhisper --config config.toml

# Or with debug logging
python -m autowhisper --config config.toml --log-level debug
```

### As systemd Service

```bash
# Start service
sudo systemctl start autowhisper@$USER

# Enable on boot
sudo systemctl enable autowhisper@$USER

# Check status
sudo systemctl status autowhisper@$USER

# View logs
journalctl -u autowhisper@$USER -f

# Stop service
sudo systemctl stop autowhisper@$USER
```

---

## Performance Optimization

### Compute Type Selection

| Compute Type | Speed | Accuracy | VRAM |
|--------------|-------|----------|------|
| float32 | Slowest | Best | Highest |
| float16 | Fast | Excellent | Medium |
| int8_float16 | Faster | Very Good | Lower |
| int8 | Fastest | Good | Lowest |

**Recommended for RTX GPUs:** `float16`

### Model Selection for Speed

| Model | Speed | Accuracy | VRAM |
|-------|-------|----------|------|
| tiny | Fastest (~20-30ms) | Good | 150MB |
| base | Fast (~40-60ms) | Very Good | 250MB |
| small | Balanced (~80-120ms) | Excellent | 500MB |
| distil-large-v3 | Balanced (~100-150ms) | Best | 1.5GB |

For most GPUs, **distil-large-v3 with float16** is recommended for best accuracy with good speed.

---

## Troubleshooting

### Build Errors

**"pip not found"**
```bash
sudo apt install python3-pip
```

**"Could not find CUDA"**
```bash
# Install CUDA toolkit
sudo apt install nvidia-cuda-toolkit

# Or download from NVIDIA
# https://developer.nvidia.com/cuda-downloads
```

**"PortAudio not found"**
```bash
sudo apt install portaudio19-dev libasound2-dev
```

### Runtime Errors

**"GPU not detected"**
```bash
# Check NVIDIA driver
nvidia-smi

# Check Python can see CUDA
python -c "import torch; print(torch.cuda.is_available())"
```

**"Model file not found"**
```bash
# Models download automatically, but you can force download:
python -c "from faster_whisper import WhisperModel; WhisperModel('distil-large-v3', device='cuda')"
```

**"No audio captured"**
```bash
# Test microphone
arecord -d 3 test.wav && aplay test.wav

# Check PulseAudio
pactl list sources short

# Restart PulseAudio
pulseaudio --kill
pulseaudio --start
```

**"Hotkey not working"**
```bash
# pynput may need special permissions
# Use the systemd service which runs with proper permissions
sudo systemctl start autowhisper@$USER
```

### Performance Issues

**Slow transcription**
- Verify GPU is being used: `nvidia-smi` while transcribing
- Use `compute_type = "int8_float16"` for faster inference
- Reduce beam_size to 1 in config

**High memory usage**
- Reduce max_duration in config
- Use smaller model
- Use `compute_type = "int8"` for lower VRAM

**Audio dropouts**
- Increase buffer_size in config
- Close other audio applications
- Check CPU usage with `htop`

---

## Development

### Running Tests

```bash
python -m pytest -v
```

### Debug Mode

```bash
python -m autowhisper --config config.toml --log-level debug
```

### Code Formatting

```bash
python -m ruff format src/
```

### Linting

```bash
python -m ruff check src/
```

---

## Uninstallation

```bash
# Stop and disable service
sudo systemctl stop autowhisper@$USER
sudo systemctl disable autowhisper@$USER

# Remove service file
sudo rm /etc/systemd/system/autowhisper@.service
sudo systemctl daemon-reload

# Remove installation
rm -rf venv/
sudo rm -rf /opt/autowhisper

# Remove models (optional, stored in ~/.cache/huggingface/)
rm -rf ~/.cache/huggingface/hub/models--*whisper*
```

---

## Next Steps

1. **Test the installation**: Press your hotkey (default: Shift+Super) and speak
2. **Customize config**: Edit `config.toml` to change hotkeys, model, output method
3. **Monitor performance**: Check logs with `journalctl -u autowhisper@$USER -f`
4. **Optimize**: Try different compute types and models for your use case

For more information, see [README.md](README.md).

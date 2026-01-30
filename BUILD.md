# Build Instructions

## Prerequisites

### System Requirements

- **OS**: Ubuntu 24.04 LTS (or similar Linux with X11)
- **GPU**: NVIDIA GPU with CUDA support
- **RAM**: 8GB minimum recommended
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
git clone https://github.com/autowhisper/autowhisper.git
cd autowhisper

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install package with dependencies
pip install -e .

# Models are downloaded automatically on first run
# Or download manually:
python -c "from faster_whisper import WhisperModel; WhisperModel('distil-small.en')"
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
6. Install systemd user service

---

## Configuration

Edit `config.toml` before running:

```toml
[model]
size = "distil-small.en"         # Model name (see table below)
device = "cuda"                   # cuda or cpu
compute_type = "bfloat16"         # bfloat16 (RTX 50xx), float16 (RTX 20-40)
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

### As systemd User Service

```bash
# Install service (run once)
./scripts/setup-daemon.sh

# Start service
systemctl --user start autowhisper

# Enable on login
systemctl --user enable autowhisper

# Check status
systemctl --user status autowhisper

# View logs
journalctl --user -u autowhisper -f

# Stop service
systemctl --user stop autowhisper

# Restart after config changes
systemctl --user restart autowhisper
```

---

## Performance Optimization

### Compute Type Selection

| Compute Type | Best For | Speed | Accuracy | VRAM |
|--------------|----------|-------|----------|------|
| bfloat16 | RTX 50xx (Blackwell) | Fastest | Excellent | Medium |
| float16 | RTX 20-40 series | Fast | Excellent | Medium |
| int8_float16 | Lower VRAM GPUs | Faster | Very Good | Lower |
| int8 | Minimal VRAM | Fast | Good | Lowest |
| float32 | CPU fallback | Slowest | Best | Highest |

**For RTX 50xx GPUs:** Use `bfloat16` for best performance.
**For RTX 20-40 GPUs:** Use `float16`.

### Model Selection

| Model | Mean Time | Speed | Accuracy | VRAM |
|-------|-----------|-------|----------|------|
| tiny.en | 78ms | 49x realtime | Good | 150MB |
| base.en | 143ms | 27x realtime | Very Good | 250MB |
| distil-small.en | 198ms | 19x realtime | Very Good | 400MB |
| small.en | 211ms | 18x realtime | Excellent | 500MB |
| distil-medium.en | 381ms | 10x realtime | Excellent | 800MB |
| distil-large-v3 | 448ms | 8.5x realtime | Best | 1.5GB |
| large-v3 | 926ms | 4.1x realtime | Best | 2GB |

**Recommended:** `distil-small.en` for best speed/accuracy balance.

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

# Run fix script if needed
sudo ./scripts/fix-nvidia.sh
```

**"Model file not found"**
```bash
# Models download automatically, but you can force download:
python -c "from faster_whisper import WhisperModel; WhisperModel('distil-small.en', device='cuda')"
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
systemctl --user start autowhisper

# Check if X11 display is set
echo $DISPLAY
```

### Performance Issues

**Slow transcription**
- Verify GPU is being used: `nvidia-smi` while transcribing
- Use appropriate compute type for your GPU
- Reduce beam_size to 1 in config
- Try a faster model

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
systemctl --user stop autowhisper
systemctl --user disable autowhisper

# Remove service file
rm ~/.config/systemd/user/autowhisper.service
systemctl --user daemon-reload

# Remove installation
rm -rf venv/

# Remove models (optional, stored in ~/.cache/huggingface/)
rm -rf ~/.cache/huggingface/hub/models--*whisper*
```

---

## Next Steps

1. **Test the installation**: Press your hotkey (default: Shift+Super) and speak
2. **Customize config**: Edit `config.toml` to change hotkeys, model, output method
3. **Monitor performance**: Check logs with `journalctl --user -u autowhisper -f`
4. **Optimize**: Try different compute types and models for your use case

For more information, see [README.md](README.md).

# Quick Start Guide

Get AutoWhisper running in 5 minutes!

## Prerequisites

- Ubuntu 24.04 LTS (or 22.04)
- NVIDIA GPU with drivers installed
- Python 3.10+
- 8GB+ RAM, 2GB+ VRAM

## Pre-Installation Check (Recommended)

Before installing, run the system compatibility check to catch any issues:

```bash
cd /home/isura/autowhisper
./scripts/check-system.sh
```

This will verify:
- NVIDIA GPU is detected and driver is working
- Sufficient RAM and VRAM
- Required system packages are installed
- Audio system is functioning

**If the check reports NVIDIA driver issues**, run the fix script:

```bash
sudo ./scripts/fix-nvidia.sh
```

Common issues it can fix:
- Driver/library version mismatch (usually needs reboot)
- Missing NVIDIA driver (auto-installs)
- Kernel module issues

## Installation

### Option 1: Automated Install (Recommended)

```bash
cd /home/isura/autowhisper
sudo ./install.sh
```

This will:
- Install all dependencies
- Create Python virtual environment
- Download the distil-large-v3 model
- Install systemd service

Takes ~5-10 minutes depending on your internet speed.

### Option 2: Manual Install

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential pkg-config libssl-dev \
    libasound2-dev portaudio19-dev xdotool xclip \
    nvidia-cuda-toolkit pulseaudio python3 python3-pip python3-venv

# Create virtual environment
cd /home/isura/autowhisper
python3 -m venv venv
source venv/bin/activate

# Install package
pip install -e .

# Model downloads automatically on first run
```

## Configuration

The default config is already optimized for NVIDIA GPUs. To customize:

```bash
nano config.toml
```

Key settings:
- `model.size = "distil-large-v3"` - Use distil-large-v3 model (best balance)
- `hotkeys.trigger = "shift+super"` - Change hotkey
- `output.method = "inject"` - Direct text injection

## Running

### Test Run (Foreground)

```bash
source venv/bin/activate
python -m autowhisper --config config.toml
```

You should see:
```
INFO AutoWhisper Daemon v0.1.0
INFO Model: distil-large-v3 (float16)
INFO Device: cuda | Hotkey: shift+super
INFO Daemon started, waiting for hotkey events...
```

**Test it:**
1. Press and hold `Shift+Super`
2. You'll hear a high-pitched beep (800Hz)
3. Speak: "This is a test"
4. Release the hotkey
5. You'll hear a low-pitched beep (400Hz)
6. After ~150ms, your text appears!

### Run as Service (Background)

```bash
# Start service
sudo systemctl start autowhisper@$USER

# Enable on boot
sudo systemctl enable autowhisper@$USER

# Check status
systemctl status autowhisper@$USER
```

## Usage

### Push-to-Talk Mode (Default)

1. **Press and hold** your hotkey (default: `Shift+Super`)
2. **Speak** clearly
3. **Release** the hotkey
4. Text appears in the active window

### Toggle Mode

Change in `config.toml`:
```toml
[hotkeys]
mode = "toggle"  # instead of "push_to_talk"
```

1. **Press once** to start recording
2. **Speak** as long as needed
3. **Press again** to stop and transcribe

### Cancel Recording

Press `Ctrl+Alt+C` to cancel current recording without transcribing.

## Performance

With your NVIDIA GPU:

| Model | Transcription Time (5s audio) | Total Latency |
|-------|-------------------------------|---------------|
| tiny | 20-30ms | 50-80ms |
| base | 40-60ms | 80-110ms |
| small | 80-120ms | 120-160ms |
| **distil-large-v3** | **100-150ms** | **150-200ms** ⭐ |

**Recommended: distil-large-v3** for best accuracy/speed balance.

## Troubleshooting

### Run the diagnostic first

```bash
./scripts/check-system.sh
```

This will identify most issues and tell you how to fix them.

### No GPU detected / Driver issues

```bash
# Check if nvidia-smi works
nvidia-smi

# If it fails, run the fix script:
sudo ./scripts/fix-nvidia.sh

# Or manually install drivers:
sudo ubuntu-drivers autoinstall
sudo reboot
```

### Hotkey not working

```bash
# Check if daemon is running
ps aux | grep autowhisper

# View logs
journalctl -u autowhisper@$USER -f
```

### No audio captured

```bash
# Test microphone
arecord -d 3 test.wav && aplay test.wav

# If silent, check PulseAudio
pactl list sources short
```

### Text not appearing

```bash
# Test xdotool
xdotool type "test"

# If fails, try clipboard mode in config.toml:
[output]
method = "clipboard"
auto_paste = true
```

### Slow transcription

```bash
# Check GPU usage while speaking
watch -n 0.1 nvidia-smi

# Try faster compute type in config.toml:
[model]
compute_type = "int8_float16"
```

## Viewing Logs

```bash
# Real-time logs
journalctl -u autowhisper@$USER -f

# Last 100 lines
journalctl -u autowhisper@$USER -n 100

# Since last boot
journalctl -u autowhisper@$USER -b
```

## Customization

### Change Model

Edit `config.toml`:
```toml
[model]
size = "small"  # tiny, base, small, medium, large-v3, distil-large-v3
```

Restart service:
```bash
sudo systemctl restart autowhisper@$USER
```

### Change Hotkey

Edit `config.toml`:
```toml
[hotkeys]
trigger = "ctrl+alt+v"  # Your custom hotkey
```

Supported keys: ctrl, alt, shift, super, space, a-z, 0-9, f1-f12

### Change Output Method

**Direct injection (default):**
```toml
[output]
method = "inject"
```

**Clipboard + auto-paste:**
```toml
[output]
method = "clipboard"
auto_paste = true
```

### Add Post-Processing

```toml
[output]
lowercase = true       # Convert to lowercase
append_newline = true  # Add newline after text
```

## Benchmarking Your System

Test performance:

```bash
# Create test audio
arecord -d 5 -f S16_LE -r 16000 -c 1 test.wav

# Run benchmark
source venv/bin/activate
python -c "
from faster_whisper import WhisperModel
import time

model = WhisperModel('distil-large-v3', device='cuda', compute_type='float16')

for i in range(3):
    start = time.time()
    segments, _ = model.transcribe('test.wav', beam_size=1)
    list(segments)
    print(f'Run {i+1}: {time.time() - start:.3f}s')
"
```

## Next Steps

- **Read [README.md](README.md)** for detailed architecture
- **Read [BUILD.md](BUILD.md)** for advanced build options
- **Customize config.toml** to your preferences
- **Try different models** (tiny for speed, distil-large-v3 for accuracy)

## Getting Help

Check logs first:
```bash
journalctl -u autowhisper@$USER -n 50
```

Common issues are usually:
1. GPU not detected → Install NVIDIA drivers
2. Hotkey not working → Check pynput permissions
3. No audio → Test microphone with `arecord`
4. Text not appearing → Try clipboard mode

Enjoy your GPU-accelerated voice-to-text! 🚀

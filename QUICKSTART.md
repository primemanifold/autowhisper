# Quick Start Guide

Get AutoWhisper running in 5 minutes.

## Prerequisites

- Ubuntu 24.04 LTS (or 22.04)
- NVIDIA GPU with drivers installed
- Python 3.10+
- 8GB+ RAM, 2GB+ VRAM

## Pre-Installation Check (Recommended)

Before installing, run the system compatibility check to catch any issues:

```bash
cd autowhisper
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
cd autowhisper
sudo ./install.sh
```

This will:
- Install all system dependencies
- Create Python virtual environment
- Download the model (on first run)
- Install systemd user service

Takes ~5-10 minutes depending on your internet speed.

### Option 2: Manual Install

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential pkg-config libssl-dev \
    libasound2-dev portaudio19-dev xdotool xclip \
    nvidia-cuda-toolkit pulseaudio python3 python3-pip python3-venv

# Create virtual environment
cd autowhisper
python3 -m venv venv
source venv/bin/activate

# Install package
pip install -e .

# Model downloads automatically on first run
```

## Configuration

The default config is optimized for NVIDIA GPUs. To customize:

```bash
nano config.toml
```

Key settings:
- `model.size` - Model to use (see performance table in README)
- `model.compute_type` - Use `bfloat16` for RTX 50xx, `float16` for RTX 20-40
- `hotkeys.trigger` - Change hotkey (default: `shift+super`)
- `output.method` - `inject` for direct typing, `clipboard` for clipboard

## Running

### Test Run (Foreground)

```bash
source venv/bin/activate
python -m autowhisper --config config.toml
```

You should see:
```
Loading configuration from config.toml
INFO AutoWhisper v2.0.0 starting
INFO Model: distil-small.en on cuda
INFO Hotkey: shift+super (push_to_talk)
```

**Test it:**
1. Press and hold `Shift+Super`
2. You'll hear a high-pitched beep (800Hz)
3. Speak: "This is a test"
4. Release the hotkey
5. You'll hear a low-pitched beep (400Hz)
6. Your text appears in the active window

### Run as Service (Background)

```bash
# Install service (if not done by install.sh)
./scripts/setup-daemon.sh

# Start service
systemctl --user start autowhisper

# Enable on login
systemctl --user enable autowhisper

# Check status
systemctl --user status autowhisper
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

- Press `Escape` to cancel (if `escape_to_cancel = true`)
- Press `Ctrl+Alt+C` to cancel current recording

## Performance

With your NVIDIA GPU and bfloat16 compute type:

| Model | Mean Time | Speed | Use Case |
|-------|-----------|-------|----------|
| tiny.en | 78ms | 49x realtime | Fastest, good accuracy |
| base.en | 143ms | 27x realtime | Fast, better accuracy |
| distil-small.en | 198ms | 19x realtime | Balanced (recommended) |
| distil-large-v3 | 448ms | 8.5x realtime | Best accuracy |

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
systemctl --user status autowhisper

# View logs
journalctl --user -u autowhisper -f
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

# Try faster model in config.toml:
[model]
size = "tiny.en"
```

## Viewing Logs

```bash
# Real-time logs
journalctl --user -u autowhisper -f

# Last 100 lines
journalctl --user -u autowhisper -n 100

# Since last boot
journalctl --user -u autowhisper -b
```

## Customization

### Change Model

Edit `config.toml`:
```toml
[model]
size = "distil-large-v3"  # For best accuracy
# or
size = "tiny.en"  # For fastest speed
```

Restart service:
```bash
systemctl --user restart autowhisper
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
also_copy_to_clipboard = true  # Also copy to clipboard
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

model = WhisperModel('distil-small.en', device='cuda', compute_type='bfloat16')

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
- **Try different models** for your speed/accuracy needs

## Getting Help

Check logs first:
```bash
journalctl --user -u autowhisper -n 50
```

Common issues are usually:
1. GPU not detected → Run `./scripts/fix-nvidia.sh`
2. Hotkey not working → Check pynput permissions
3. No audio → Test microphone with `arecord`
4. Text not appearing → Try clipboard mode

Enjoy your GPU-accelerated voice-to-text!

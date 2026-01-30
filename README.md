# AutoWhisper

**GPU-Accelerated Voice-to-Text Daemon for Ubuntu**

A zero-UI, background daemon that converts speech to text using NVIDIA GPU acceleration with faster-whisper. Triggered by keyboard shortcuts with audio feedback, delivering transcribed text directly to any application.

---

## Key Features

- **Sub-500ms latency** - GPU-accelerated inference with faster-whisper
- **Zero UI** - Runs entirely in background as a systemd user service
- **Hotkey triggered** - Customizable keyboard shortcuts (push-to-talk or toggle mode)
- **Audio feedback** - Beeps indicate recording start/stop/error
- **Instant output** - Text appears directly in active window via xdotool or clipboard
- **100% offline** - No network requests, complete privacy
- **RTX optimized** - Tuned for NVIDIA GPUs with bfloat16/float16 support

---

## Performance

Benchmarks on RTX 5070 Laptop with bfloat16:

| Model | Mean | Min | Speed | Accuracy |
|-------|------|-----|-------|----------|
| tiny.en | 78ms | 19ms | 49x realtime | Good |
| base.en | 143ms | 90ms | 27x realtime | Very Good |
| distil-small.en | 198ms | 138ms | 19x realtime | Very Good |
| small.en | 211ms | 100ms | 18x realtime | Excellent |
| distil-medium.en | 381ms | 158ms | 10x realtime | Excellent |
| **distil-large-v3** | **448ms** | **276ms** | **8.5x realtime** | **Best** |
| large-v3-turbo | 657ms | 346ms | 5.8x realtime | Best |

**Recommended:** `distil-small.en` for speed, `distil-large-v3` for accuracy.

---

## Quick Start

### Prerequisites

- Ubuntu 24.04 LTS (or 22.04 with X11)
- NVIDIA GPU with CUDA support
- Python 3.10+
- 8GB+ RAM, 2GB+ VRAM

### Installation

```bash
# Clone repository
git clone https://github.com/autowhisper/autowhisper.git
cd autowhisper

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -e .

# Start the daemon
python -m autowhisper --config config.toml
```

See [QUICKSTART.md](QUICKSTART.md) for detailed installation with systemd service.

### Usage

**Push-to-Talk Mode (Default):**
1. **Press and hold** `Shift+Super` (default hotkey)
2. **Speak** clearly
3. **Release** the hotkey
4. Text appears in active window

**Toggle Mode:**
1. **Press once** to start recording
2. **Speak** as long as needed
3. **Press again** to stop and transcribe

**Cancel Recording:** Press `Escape` or `Ctrl+Alt+C`

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   AutoWhisper Daemon                │
│                                                     │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐     │
│  │  Hotkey  │───▶│  Audio   │───▶│ Whisper  │     │
│  │ Listener │    │ Capture  │    │ Inference│     │
│  └──────────┘    └──────────┘    └──────────┘     │
│       │               │                 │          │
│       │               ▼                 ▼          │
│       │          ┌──────────┐    ┌──────────┐     │
│       │          │ Feedback │    │  Output  │     │
│       └─────────▶│  Beeps   │    │   Text   │     │
│                  └──────────┘    └──────────┘     │
│                                        │          │
└────────────────────────────────────────┼──────────┘
                                         │
                                         ▼
                                  Active Window
```

### Core Components

| Module | Technology | Purpose |
|--------|-----------|---------|
| **Audio Capture** | sounddevice | Low-latency 16kHz mono recording |
| **VAD** | Silero VAD | Voice activity detection, silence trimming |
| **Inference** | faster-whisper + CUDA | GPU-accelerated transcription |
| **Hotkeys** | pynput | Global keyboard hooks |
| **Output** | xdotool/xclip | Text injection to active window |
| **Feedback** | sounddevice | Audio beep generation |
| **Config** | TOML | Runtime configuration |

---

## Project Structure

```
autowhisper/
├── src/autowhisper/
│   ├── __init__.py       # Package init, version
│   ├── __main__.py       # Entry point & CLI
│   ├── config.py         # Configuration loader
│   ├── audio.py          # Audio capture & VAD
│   ├── inference.py      # faster-whisper GPU inference
│   ├── hotkey.py         # Global hotkey listener
│   ├── output.py         # Text injection & clipboard
│   ├── feedback.py       # Beep generation
│   └── daemon.py         # State machine & event loop
├── scripts/
│   ├── check-system.sh   # System compatibility checker
│   ├── fix-nvidia.sh     # NVIDIA driver fixer
│   ├── setup-daemon.sh   # systemd service setup
│   └── *.sh              # Various utility scripts
├── tests/                # Unit and integration tests
├── models/               # Whisper models (downloaded automatically)
├── config.toml           # Runtime configuration
├── pyproject.toml        # Python package definition
├── autowhisper.service   # systemd user service file
├── install.sh            # Automated installer
└── Makefile              # Build shortcuts
```

---

## Configuration

Edit `config.toml` to customize behavior:

### Model Settings

```toml
[model]
size = "distil-small.en"  # See model table above
device = "cuda"           # cuda, cpu, or auto
compute_type = "bfloat16" # bfloat16 (RTX 50xx), float16 (RTX 20-40), int8_float16, int8
beam_size = 1             # 1 = fastest, 5 = more accurate
language = "en"           # or "auto" for detection
num_threads = 4           # CPU threads for preprocessing
```

**Compute Type Selection:**

| Compute Type | Best For | Speed | VRAM |
|--------------|----------|-------|------|
| bfloat16 | RTX 50xx series | Fastest | Medium |
| float16 | RTX 20-40 series | Fast | Medium |
| int8_float16 | Lower VRAM GPUs | Faster | Lower |
| int8 | Minimal VRAM | Fast | Lowest |

### Hotkey Settings

```toml
[hotkeys]
mode = "push_to_talk"     # or "toggle"
trigger = "shift+super"   # Start/stop recording
cancel = "ctrl+alt+c"     # Cancel current recording
escape_to_cancel = true   # Allow Escape to cancel
```

**Supported keys:** ctrl, alt, shift, super, space, a-z, 0-9, f1-f12

### Output Settings

```toml
[output]
method = "inject"              # "inject" (xdotool) or "clipboard"
also_copy_to_clipboard = true  # Copy to clipboard when using inject
auto_paste = true              # Auto Ctrl+V after clipboard copy
paste_delay = 0.05             # Delay before paste (seconds)
lowercase = false              # Convert to lowercase
append_newline = true          # Add newline after text
```

### Audio Settings

```toml
[audio]
sample_rate = 16000       # Whisper native rate (don't change)
buffer_size = 1024        # ~64ms latency
vad_enabled = true        # Trim silence (Silero VAD)
vad_threshold = 0.5       # 0.0-1.0 (higher = stricter)
silence_duration = 0.3    # Silence to trim (seconds)
max_duration = 60.0       # Max recording length (seconds)
```

### Feedback Settings

```toml
[feedback]
enabled = true            # Enable audio beeps
frequency_start = 800     # Start beep frequency (Hz)
frequency_stop = 400      # Stop beep frequency (Hz)
frequency_error = 600     # Error beep frequency (Hz)
duration = 0.1            # Beep duration (seconds)
volume = 0.3              # 0.0 - 1.0
```

---

## Running

### Foreground (Testing)

```bash
source venv/bin/activate
python -m autowhisper --config config.toml

# With debug logging
python -m autowhisper --config config.toml --log-level debug

# Override model
python -m autowhisper --config config.toml --model tiny.en
```

### Background (systemd User Service)

```bash
# Install service
./scripts/setup-daemon.sh

# Start service
systemctl --user start autowhisper

# Enable on login
systemctl --user enable autowhisper

# View logs
journalctl --user -u autowhisper -f

# Status
systemctl --user status autowhisper

# Restart after config changes
systemctl --user restart autowhisper

# Stop
systemctl --user stop autowhisper
```

---

## Utility Scripts

| Script | Purpose |
|--------|---------|
| `scripts/check-system.sh` | Check system compatibility (GPU, RAM, packages) |
| `scripts/fix-nvidia.sh` | Fix common NVIDIA driver issues |
| `scripts/setup-daemon.sh` | Install systemd user service |
| `scripts/restart-daemon.sh` | Restart the daemon service |
| `scripts/stop-daemon.sh` | Stop the daemon service |
| `scripts/logs-daemon.sh` | View daemon logs |
| `scripts/status-daemon.sh` | Check daemon status |
| `scripts/apply-config.sh` | Apply config changes and restart |

---

## Troubleshooting

### Run the diagnostic script first

```bash
./scripts/check-system.sh
```

This checks GPU, drivers, RAM, and system packages.

### No GPU detected / Driver issues

```bash
# Run the automated fix script
sudo ./scripts/fix-nvidia.sh

# Or manually:
nvidia-smi  # Check if driver works

# Install if needed
sudo ubuntu-drivers autoinstall
sudo reboot
```

### Hotkey not working

```bash
# Check service is running
systemctl --user status autowhisper

# View logs
journalctl --user -u autowhisper -n 50
```

### No audio captured

```bash
# Test microphone
arecord -d 3 test.wav && aplay test.wav

# Check PulseAudio
pactl list sources short
```

### Text not appearing

```bash
# Test xdotool
xdotool type "test"

# Try clipboard mode in config.toml:
[output]
method = "clipboard"
auto_paste = true
```

### Slow transcription

```bash
# Verify GPU usage while recording
watch -n 0.1 nvidia-smi

# Try faster model in config.toml:
[model]
size = "tiny.en"
```

See [BUILD.md](BUILD.md) for more troubleshooting.

---

## Development

### Running Tests

```bash
source venv/bin/activate
python -m pytest -v

# With coverage
python -m pytest --cov=autowhisper
```

### Code Formatting

```bash
python -m ruff format src/
python -m ruff check src/
```

### Debug Build

```bash
python -m autowhisper --config config.toml --log-level debug
```

See [TESTING.md](TESTING.md) for comprehensive testing guide.

---

## Documentation

- **[QUICKSTART.md](QUICKSTART.md)** - Get started in 5 minutes
- **[BUILD.md](BUILD.md)** - Detailed build instructions
- **[TESTING.md](TESTING.md)** - Testing guide
- **[PACKAGING.md](PACKAGING.md)** - Creating distribution packages
- **[DISTRIBUTION.md](DISTRIBUTION.md)** - Distribution guide

---

## Privacy & Security

### Privacy Guarantees

- **100% offline** - No network requests
- **No storage** - Audio deleted immediately after transcription
- **No telemetry** - Zero data collection
- **Open source** - Audit the code yourself

### Security Considerations

- Runs as user service (systemd user instance)
- PID file prevents multiple instances
- Input validation on all configuration
- Resource limits in systemd service (6GB RAM, 400% CPU)

---

## Roadmap

### Phase 1: Core Functionality (Complete)

- [x] Audio capture with VAD
- [x] GPU-accelerated inference
- [x] Global hotkey support (push-to-talk and toggle)
- [x] Text injection and clipboard output
- [x] Audio feedback
- [x] Configuration system
- [x] systemd user service
- [x] Diagnostic and fix scripts

### Phase 2: Enhancements (Future)

- [ ] Wayland support (ydotool)
- [ ] Streaming mode (real-time transcription)
- [ ] Command mode (voice commands)
- [ ] Custom vocabulary
- [ ] Multiple language support
- [ ] Usage statistics
- [ ] GUI configuration tool

---

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Run `ruff format` and `ruff check`
6. Submit a pull request

---

## License

MIT License - See [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- **[OpenAI Whisper](https://github.com/openai/whisper)** - Speech recognition model
- **[faster-whisper](https://github.com/guillaumekln/faster-whisper)** - CTranslate2 optimization
- **[Silero VAD](https://github.com/snakers4/silero-vad)** - Voice activity detection
- **[pynput](https://github.com/moses-palmer/pynput)** - Global input hooks

---

## Support

- **Issues:** Report bugs on GitHub Issues
- **Logs:** Check `journalctl --user -u autowhisper -f`
- **Diagnostics:** Run `./scripts/check-system.sh`

---

**AutoWhisper v2.0.0** - Built with Python and faster-whisper

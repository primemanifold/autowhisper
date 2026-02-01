# Build Instructions

## Prerequisites

```bash
# System dependencies
sudo apt install -y build-essential pkg-config libssl-dev git \
    libasound2-dev portaudio19-dev pulseaudio \
    xdotool xclip nvidia-cuda-toolkit python3 python3-pip python3-venv

# Verify CUDA
nvidia-smi && nvcc --version
```

## Build

```bash
git clone https://github.com/autowhisper/autowhisper.git
cd autowhisper
python3 -m venv venv && source venv/bin/activate
pip install -e .
```

Or use the installer: `sudo ./install.sh`

## Configuration

Edit `config.toml`. Key settings:

| Setting | Options | Description |
|---------|---------|-------------|
| model.size | tiny.en, distil-small.en, distil-large-v3 | Model selection |
| model.compute_type | bfloat16, float16, int8 | bfloat16 for RTX 50xx, float16 for RTX 20-40 |
| hotkeys.trigger | shift+super, ctrl+alt+v | Recording hotkey |
| output.method | inject, clipboard | Text injection method |

## Running

**Foreground:**
```bash
source venv/bin/activate
python -m autowhisper --config config.toml
```

**As service:**
```bash
./scripts/setup-daemon.sh
systemctl --user start autowhisper
systemctl --user enable autowhisper
```

## Troubleshooting

| Error | Solution |
|-------|----------|
| pip not found | `sudo apt install python3-pip` |
| CUDA not found | `sudo apt install nvidia-cuda-toolkit` |
| PortAudio not found | `sudo apt install portaudio19-dev libasound2-dev` |
| GPU not detected | Run `nvidia-smi`, then `sudo ./scripts/fix-nvidia.sh` |
| No audio | Test with `arecord -d 3 test.wav && aplay test.wav` |

## Development

```bash
python -m pytest -v              # Run tests
python -m ruff format src/       # Format code
python -m ruff check src/        # Lint
```

## Uninstall

```bash
systemctl --user stop autowhisper
systemctl --user disable autowhisper
rm ~/.config/systemd/user/autowhisper.service
systemctl --user daemon-reload
rm -rf venv/
```

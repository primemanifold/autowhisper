# Quick Start

## Prerequisites

- Ubuntu 22.04+ with X11
- NVIDIA GPU with CUDA
- Python 3.10+

## Install

```bash
git clone https://github.com/autowhisper/autowhisper.git
cd autowhisper
python3 -m venv venv && source venv/bin/activate
pip install -e .
```

Or use the automated installer: `sudo ./install.sh`

## Test

```bash
python -m autowhisper --config config.toml
```

Hold `Shift+Super`, speak, release. Text appears where your cursor is.

## Run as Service

```bash
./scripts/setup-daemon.sh
systemctl --user start autowhisper
systemctl --user enable autowhisper
```

## Change Hotkey

Run `./configure-hotkey.sh` or edit `config.toml`:

```toml
[hotkeys]
trigger = "ctrl+alt+v"
```

Then restart: `systemctl --user restart autowhisper`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No GPU | `sudo ./scripts/fix-nvidia.sh` |
| Hotkey not working | Check logs: `journalctl --user -u autowhisper -f` |
| No audio | Test mic: `arecord -d 3 test.wav && aplay test.wav` |
| Text not appearing | Try `method = "clipboard"` in config.toml |

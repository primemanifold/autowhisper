# AutoWhisper CLI and PPA Distribution Design

## Overview

Replace bash scripts with a unified `autowhisper` CLI tool and distribute via Launchpad PPA.

## CLI Commands

```
autowhisper start                  # Start daemon (systemd)
autowhisper stop                   # Stop daemon
autowhisper restart                # Restart daemon
autowhisper status                 # Show status + health
autowhisper logs                   # Tail logs (journalctl)

autowhisper config                 # Show current config
autowhisper config edit            # Open in $EDITOR
autowhisper config set KEY VALUE   # Set a value

autowhisper doctor                 # Diagnose issues
autowhisper doctor --fix           # Attempt fixes

autowhisper model list             # List available models
autowhisper model download MODEL   # Download a model
```

## Installation Layout

```
/usr/bin/autowhisper                       # CLI entry point
/usr/lib/autowhisper/                      # Python package + venv
/etc/autowhisper/config.toml               # System config
/usr/lib/systemd/user/autowhisper.service  # User service

~/.cache/autowhisper/models/               # Downloaded models
~/.config/autowhisper/config.toml          # User override (optional)
```

## Source Structure

New files in `src/autowhisper/`:

```
cli.py      # Click CLI commands
doctor.py   # System diagnostics
models.py   # Model download/management
```

Entry point in pyproject.toml:
```toml
[project.scripts]
autowhisper = "autowhisper.cli:cli"
```

## Doctor Command Checks

1. GPU detection (nvidia-smi)
2. CUDA availability (ctranslate2.get_cuda_device_count)
3. Audio device detection
4. X11/display access
5. Required tools (xdotool, xclip)
6. Model downloaded
7. Service status

## Distribution

**Target:** Launchpad PPA

**User installation:**
```bash
sudo add-apt-repository ppa:yourname/autowhisper
sudo apt install autowhisper
autowhisper doctor
autowhisper start
```

**Dependencies (debian/control):**
- nvidia-cuda-toolkit (recommends)
- xdotool, xclip
- python3 (>= 3.10)
- libportaudio2

## First-Run Experience

```bash
sudo apt install autowhisper
autowhisper doctor              # Check system
autowhisper model download distil-small.en
autowhisper start
# Press Shift+Super, speak, done
```

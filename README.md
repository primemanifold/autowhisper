# AutoWhisper

GPU-accelerated voice-to-text for Ubuntu. Press a hotkey, speak, release — text appears.

## Install

```bash
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update
sudo apt install autowhisper
```

## Quick Start

```bash
autowhisper doctor              # check system requirements
systemctl --user enable --now autowhisper
```

Press `Ctrl+Shift+Space`, speak, release. Text appears at your cursor.

## Usage

```bash
autowhisper doctor              # diagnose system
autowhisper config              # open settings GUI
autowhisper model list          # show available models
autowhisper model download <name>  # download a model
```

View logs:
```bash
journalctl --user -u autowhisper -f
```

## Install from Source

```bash
git clone https://github.com/rabotinc/autowhisper.git
cd autowhisper
python3 -m venv venv && source venv/bin/activate
pip install -e .
python -m autowhisper --config config.toml
```

## Configure

Run `autowhisper config` to open the settings GUI, or edit the config file directly:

```bash
~/.config/autowhisper/config.toml   # user config
/etc/autowhisper/config.toml        # system default
```

Example:
```toml
[model]
size = "distil-small.en"  # tiny.en (fastest) to distil-large-v3 (best)
compute_type = "bfloat16" # bfloat16 (RTX 50xx), float16 (RTX 20-40)

[hotkeys]
mode = "push_to_talk"     # or "toggle"
trigger = "ctrl+shift+space"
```

## Troubleshooting

```bash
autowhisper doctor               # diagnose issues
journalctl --user -u autowhisper -f  # view logs
```

## Models

| Model | Speed | Accuracy |
|-------|-------|----------|
| tiny.en | 78ms | Good |
| distil-small.en | 198ms | Very Good |
| distil-large-v3 | 448ms | Best |

## License

Apache 2.0

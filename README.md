# AutoWhisper

GPU-accelerated voice-to-text for Ubuntu. Press a hotkey, speak, release — text appears.

## Install

```bash
git clone https://github.com/autowhisper/autowhisper.git
cd autowhisper
python3 -m venv venv && source venv/bin/activate
pip install -e .
```

## Run

```bash
python -m autowhisper --config config.toml
```

Press `Shift+Super`, speak, release. Text appears in the active window.

## Run as Service

```bash
./scripts/setup-daemon.sh
systemctl --user start autowhisper
systemctl --user enable autowhisper  # start on login
```

## Configure

Edit `config.toml`:

```toml
[model]
size = "distil-small.en"  # tiny.en (fastest) to distil-large-v3 (best)
compute_type = "bfloat16" # bfloat16 (RTX 50xx), float16 (RTX 20-40)

[hotkeys]
mode = "push_to_talk"     # or "toggle"
trigger = "shift+super"
```

Run `./configure-hotkey.sh` to set hotkeys interactively.

## Troubleshooting

```bash
./scripts/check-system.sh      # diagnose issues
sudo ./scripts/fix-nvidia.sh   # fix GPU problems
journalctl --user -u autowhisper -f  # view logs
```

## Models

| Model | Speed | Accuracy |
|-------|-------|----------|
| tiny.en | 78ms | Good |
| distil-small.en | 198ms | Very Good |
| distil-large-v3 | 448ms | Best |

## License

MIT

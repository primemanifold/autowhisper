# AutoWhisper

GPU-accelerated voice-to-text for Ubuntu. Press a hotkey, speak, release — text appears at your cursor.

Native C++ application powered by [whisper.cpp](https://github.com/ggerganov/whisper.cpp). Runs entirely offline.

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

Press `Shift+Super`, speak, release. Text appears at your cursor.

## Usage

```bash
autowhisper doctor              # diagnose system
autowhisper config              # open settings GUI
autowhisper model list          # show available models
autowhisper model download <name>  # download a model
autowhisper run                 # run in foreground
```

View logs:
```bash
journalctl --user -u autowhisper -f
```

## Build from Source

### Dependencies (Ubuntu)

```bash
sudo apt install cmake g++ pkg-config \
  libx11-dev libxtst-dev libxext-dev libxi-dev libxrandr-dev \
  libgtk-3-dev libayatana-appindicator3-dev libpulse-dev libssl-dev
```

### Build

```bash
git clone https://github.com/rabotinc/autowhisper.git
cd autowhisper
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The binary is at `build/autowhisper`.

### Options

| CMake Option | Default | Description |
|---|---|---|
| `AUTOWHISPER_ENABLE_CUDA` | OFF | Enable CUDA GPU acceleration via whisper.cpp |
| `AUTOWHISPER_ENABLE_TESTS` | ON | Build the Catch2 test suite |

```bash
# Build with CUDA support
cmake -B build -DCMAKE_BUILD_TYPE=Release -DAUTOWHISPER_ENABLE_CUDA=ON
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure
```

## Configure

Run `autowhisper config` to open the settings GUI, or edit the config file directly:

```
~/.config/autowhisper/config.toml   # user config
/etc/autowhisper/config.toml        # system default
```

Example:
```toml
[model]
size = "distil-small.en"  # tiny.en (fastest) to distil-large-v3 (best)
device = "cuda"           # cuda, cpu, or auto
compute_type = "bfloat16" # bfloat16 (RTX 50xx), float16 (RTX 20-40xx)

[hotkeys]
mode = "push_to_talk"     # or "toggle"
trigger = ["shift+super"]

[output]
method = "inject"         # or "clipboard"
ending_action = "none"    # none, newline, or return_key
```

## Models

| Model | Speed | Accuracy |
|-------|-------|----------|
| tiny.en | 78ms | Good |
| distil-small.en | 198ms | Very Good |
| distil-large-v3 | 448ms | Best |

## Troubleshooting

```bash
autowhisper doctor                     # diagnose issues
journalctl --user -u autowhisper -f    # view logs
```

## License

Apache 2.0

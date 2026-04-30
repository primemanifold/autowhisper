# AutoWhisper Architecture

Generated: 2026-04-30T14:28:38Z
Repo: `primemanifold/autowhisper`
Branch: `core`
Observed commit: `2778945`

## Product snapshot

AutoWhisper is a native C++20, Linux/X11-first voice-to-text desktop application. The core user workflow is: press the configured hotkey, record speech, release the hotkey, transcribe locally with whisper.cpp, and insert the resulting text at the active cursor. The README positions it as GPU-accelerated and entirely offline for Ubuntu users.

## Top-level structure

- `src/` — C++ application code.
- `tests/cpp/` — Catch2 tests for config, schema, settings HTTP handlers, sidecar behavior, subprocess helpers, tray display formatting, and models.
- `src/settings/web/` — embedded browser settings UI assets.
- `cmake/` — dependency, warning, platform, and web-asset embedding helpers.
- `debian/`, `autowhisper.service`, `build-deb.sh` — Ubuntu packaging and systemd user service assets.
- `docs/plans/` — historical and active design/implementation plans.
- `models/` — placeholder for local model files, ignored except `.gitkeep`.
- `.github/workflows/` — CI and PPA release workflows.

## Build model

`CMakeLists.txt` defines two main targets:

1. `autowhisper_core` static library, containing platform-independent and unit-testable modules:
   - `src/config/config.cpp`
   - `src/config/schema.cpp`
   - `src/hotkey/hotkey_common.cpp`
   - `src/models/models.cpp`
   - `src/settings/handlers.cpp`
   - `src/settings/sidecar.cpp`
   - `src/tray/tray_common.cpp`
   - `src/util/logging.cpp`
   - `src/util/subprocess.cpp`

2. `autowhisper` executable, containing CLI, daemon, audio, inference, feedback, doctor, output, and platform-specific hotkey/output/tray implementations.

Important dependencies are vendored as Git submodules under `deps/`:

- `whisper.cpp` for inference.
- `miniaudio` for audio capture and feedback playback.
- `CLI11` for CLI command parsing.
- `tomlplusplus` for TOML config parsing.
- `spdlog` for logging.
- `Catch2` for C++ tests.
- `cpp-httplib` for the local settings HTTP server.
- `nlohmann-json` for settings API JSON.

## Runtime entry points

### CLI entry point

`src/main.cpp` creates a `CLI::App`, delegates command registration to `autowhisper::setup_cli`, parses args, and prints help when no subcommand is provided.

`src/cli/cli.cpp` registers these command groups:

- Service lifecycle: `start`, `stop`, `restart`, `status`, `logs` via `systemctl --user` and `journalctl`.
- Foreground app: `run` loads config, applies CLI overrides, initializes logging, constructs `AutoWhisperDaemon`, and enters the daemon loop.
- Config: `config`, `config show`, `config edit`, `config set`, `config path`, `config ui`.
- Diagnostics: `doctor`.
- Models: `model list`, `model download`.

### Daemon lifecycle

`AutoWhisperDaemon` in `src/daemon/daemon.cpp` owns the runtime components:

1. Signal handling and PID file.
2. `AudioManager` for recording.
3. `WhisperInference` for local speech recognition.
4. `OutputManager` for text injection or clipboard output.
5. `FeedbackManager` for audio cues.
6. `PulseAudioManager` for optional muting of other apps.
7. `HotkeyManager` for global hotkey events.
8. `TrayManager` for status and quit/config menu.

The daemon state machine is effectively:

```text
IDLE --START--> RECORDING --STOP--> PROCESSING --done/error--> IDLE
                      |--CANCEL-----------------------------> IDLE
```

`handle_start()` begins recording, updates tray state, plays start feedback, and optionally mutes other apps. `handle_stop()` stops recording, trims silence, transcribes, injects output, handles errors, and returns to idle.

## Audio and inference flow

```text
Hotkey START
  -> AudioManager::start_recording()
  -> miniaudio capture callback appends mono float samples
Hotkey STOP
  -> AudioManager::stop_recording()
  -> AudioManager::trim_silence()
  -> WhisperInference::transcribe(samples)
  -> OutputManager::inject(text)
```

`WhisperInference` loads GGML model files from the local cache. `src/models/models.cpp` defines the supported model catalog, model download URLs, sizes, and claimed speeds.

## Output flow

The output abstraction supports two user-visible methods:

- `inject` — direct text injection into the active application.
- `clipboard` — copy text to clipboard, optionally auto-paste.

On Linux/X11, output uses external tools such as `xdotool` and `xclip` through subprocess helpers. macOS and Windows output files exist as stubs/placeholders rather than product-ready platform implementations.

## Hotkey flow

`HotkeyManager` parses configured key combinations in common code, then delegates platform registration. Linux uses X11/XRecord/XTest style platform files. macOS and Windows hotkey platform files are present but not product-complete.

## Configuration architecture

Config sources:

- User config: `~/.config/autowhisper/config.toml`.
- System default: `/etc/autowhisper/config.toml`.
- Repo/default example: `config.toml`.

`Config::load()` parses TOML, merges with defaults, and validates. `src/config/schema.{h,cpp}` is the typed catalog of config sections, keys, enum values, numeric bounds, and descriptions. The schema is used by validation and exposed to the settings UI.

Notable config categories:

- `audio`: capture format, VAD, silence trimming.
- `daemon`: logging, PID/work dir.
- `feedback`: beep settings.
- `hotkeys`: trigger/cancel keys and push-to-talk vs toggle mode.
- `model`: Whisper model, device, compute type, language, threads, beam size.
- `output`: injection/clipboard behavior.
- `tray`: tray enablement.

## Settings UI architecture

The settings UI is a local browser app launched by `autowhisper config ui`.

```text
autowhisper config ui [--config PATH] [--no-browser]
  -> resolve config path
  -> acquire per-config sidecar/flock single-instance lock
  -> bind cpp-httplib server to 127.0.0.1:0
  -> write sidecar with pid/port/path
  -> optionally launch browser
  -> serve embedded HTML/CSS/JS and JSON APIs
```

HTTP surface, from the design docs and code:

- `GET /` — embedded `index.html`.
- `GET /style.css` — embedded CSS.
- `GET /app.js` — embedded JS.
- `GET /api/schema` — config schema JSON.
- `GET /api/defaults` — default config JSON.
- `GET /api/config` — effective config JSON.
- `PUT /api/config` — validate and save config TOML.

The browser UI dynamically renders form controls from `schema` and current config values.

## Platform support reality

The product is currently Linux/X11-first. Evidence:

- README says Ubuntu.
- Debian/PPA packaging is first-class.
- CI runs on Ubuntu 24.04.
- Runtime doctor checks NVIDIA, CUDA, PulseAudio/ALSA, X11, `xdotool`, `xclip`, and systemd user service.
- macOS/Windows source files exist for compile-time boundaries, but are not equivalent supported product paths.

## CI and release

`.github/workflows/ci.yml` runs on pushes and PRs targeting `core`, builds on Ubuntu 24.04, installs system dependencies, configures CMake, builds, and runs `ctest`.

`.github/workflows/ppa-release.yml` publishes source packages to the configured Launchpad PPA on `v*` tags after version verification and GPG signing.

## Test architecture

Catch2 tests cover the platform-independent core, especially:

- Config loading, defaults, validation, and path behavior.
- Hotkey key-combo parsing.
- Logging setup behavior.
- Model catalog and download helper behavior.
- Schema definitions and JSON serialization.
- Settings handlers and HTTP API behavior.
- Settings sidecar parse/format/path behavior.
- Subprocess helpers.
- Tray display formatting.

The full test command intended by docs is:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## Documentation contradictions and drift found during Phase 0

1. `README.md` says `autowhisper config` opens the settings GUI, but current CLI code routes bare `config` to `cmd_config_edit()` and the browser UI is `autowhisper config ui`.
2. The settings UI design doc says no-flag `config ui` should target the user config path, but implementation appears to resolve through default config discovery, which may select repo/system config.
3. The settings UI progress handoff says all 28 tasks are complete, but retains an archived “remaining tasks” table. It is labeled historical, but can still confuse future operators.
4. `docs/hotkey-resume-deadlock.md` references a removed GTK settings dialog and appears stale unless the defect was revalidated against the browser settings UI.
5. The older stability milestone intentionally removed GUI settings, while the later settings rewrite reintroduced a browser settings UI. That is a real product direction change and should be captured in ADR/state.
6. Product messaging says “GPU-accelerated,” while `AUTOWHISPER_ENABLE_CUDA` defaults to `OFF` in CMake. This can be true for packaged CUDA builds, but the source-build path needs clearer wording.

## Current product hypothesis

AutoWhisper is an offline, local-first dictation tool for Ubuntu/X11 users who want hotkey-triggered speech-to-text inserted directly into any desktop app, optimized around local Whisper/Distil-Whisper models, optional NVIDIA/CUDA acceleration, low-latency push-to-talk ergonomics, and simple systemd/PPA distribution.

## Phase 0 accuracy status

This architecture map is grounded in repository inspection at commit `2778945`, plus an independent read-only review. It should be revisited after major changes to the CLI/config UI, platform support, or runtime pipeline.

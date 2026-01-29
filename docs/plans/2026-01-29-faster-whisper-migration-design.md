# Faster-Whisper Migration Design

## Overview

Migrate autowhisper from whisper-rs (Rust/whisper.cpp) to faster-whisper (Python) with distil-large-v3 for ~4-6x inference speedup.

| Metric | Current | Target |
|--------|---------|--------|
| Backend | whisper.cpp (Rust) | faster-whisper (Python) |
| Model | ggml-small.bin | distil-large-v3 |
| Quantization | float16 | int8_float16 |
| Expected RTF | ~25x | ~100x |
| 10s audio inference | ~400ms | ~100ms |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     autowhisper.py                          │
│                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │   HotkeyMgr  │    │  AudioMgr    │    │  Whisper     │  │
│  │   (pynput)   │    │ (sounddevice)│    │ (faster-wh)  │  │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘  │
│         │                   │                   │          │
│         └───────────────────┼───────────────────┘          │
│                             ▼                              │
│                    ┌────────────────┐                      │
│                    │     Daemon     │                      │
│                    │ (State Machine)│                      │
│                    └────────┬───────┘                      │
│                             │                              │
│         ┌───────────────────┼───────────────────┐          │
│         ▼                   ▼                   ▼          │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │   OutputMgr  │    │  FeedbackMgr │    │   VAD        │  │
│  │ (xlib/xdotool)│   │ (simpleaudio)│    │ (silero)     │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Module Breakdown

| Module | File | Responsibility |
|--------|------|----------------|
| Config | `config.py` | TOML parsing, validation, defaults |
| Hotkey | `hotkey.py` | Global keyboard listener, event queue |
| Audio | `audio.py` | Recording, silence trimming, VAD integration |
| Inference | `inference.py` | faster-whisper model loading, transcription |
| Output | `output.py` | X11 injection, xdotool fallback, clipboard |
| Feedback | `feedback.py` | Beep generation and playback |
| Daemon | `daemon.py` | State machine, event loop orchestration |
| Main | `__main__.py` | Entry point, CLI args, daemon startup |

## Threading Model

```
┌─────────────────────────────────────────────────────────────┐
│  Main Thread                                                │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Daemon Event Loop                                     │ │
│  │  - Processes hotkey events from queue                  │ │
│  │  - Manages state transitions                           │ │
│  │  - Coordinates audio capture start/stop                │ │
│  │  - Triggers inference and output                       │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
         ▲                              ▲
         │ queue.Queue                  │ threading.Event
         │                              │
┌────────┴────────┐            ┌────────┴────────┐
│  Hotkey Thread  │            │  Audio Thread   │
│  (pynput)       │            │  (sounddevice   │
│  - Blocks on    │            │   callback)     │
│    keyboard     │            │  - Fills buffer │
│  - Sends events │            │  - Runs VAD     │
└─────────────────┘            └─────────────────┘
```

## Data Flow

1. User presses hotkey → `HotkeyMgr` sends `START` to queue
2. Daemon transitions `Idle → Recording`, plays start beep
3. `AudioMgr` begins capturing to numpy buffer, Silero VAD runs per chunk
4. User releases hotkey → `HotkeyMgr` sends `STOP` to queue
5. Daemon transitions `Recording → Processing`, plays stop beep
6. Audio buffer passed to `Whisper.transcribe()` (GPU inference)
7. Text result passed to `OutputMgr.inject(text)`
8. Daemon transitions `Processing → Idle`

## Technology Choices

| Component | Library | Rationale |
|-----------|---------|-----------|
| Audio capture | sounddevice | NumPy-native, simple API, ~50ms latency |
| VAD | Silero VAD | Neural network-based, accurate, ~5ms latency |
| Text output | python-xlib + xdotool | Direct X11 for speed, xdotool as fallback |
| Beeps | simpleaudio + numpy | Lightweight, numpy already required |
| Hotkeys | pynput | Well-maintained, cross-platform |
| Inference | faster-whisper | CTranslate2, superior CUDA optimization |

## Configuration

```toml
[model]
size = "distil-large-v3"
device = "cuda"
compute_type = "int8_float16"
beam_size = 1
language = "en"

[audio]
sample_rate = 16000
channels = 1
buffer_size = 512
vad_enabled = true
vad_threshold = 0.5
silence_duration = 0.3
max_duration = 60.0

[hotkeys]
mode = "push_to_talk"
trigger = "shift+super"
cancel = "ctrl+alt+c"

[output]
method = "inject"
auto_paste = true
paste_delay = 0.05
append_newline = false
lowercase = false

[feedback]
enabled = true
frequency_start = 800
frequency_stop = 400
frequency_error = 600
duration = 0.1
volume = 0.3

[daemon]
log_level = "info"
pid_file = "/tmp/autowhisper.pid"
```

## Dependencies

```
faster-whisper>=1.0.0
silero-vad>=4.0.0
sounddevice>=0.4.6
numpy>=1.24.0
pynput>=1.7.6
python-xlib>=0.33
simpleaudio>=1.0.4
toml>=0.10.2
```

## Error Handling

| Error | Detection | Recovery |
|-------|-----------|----------|
| Model load failure | Exception on startup | Log error, exit with code 1 |
| CUDA unavailable | `torch.cuda.is_available()` | Fall back to CPU, warn user |
| Audio device error | sounddevice exception | Retry 3x, then error beep + log |
| Hotkey conflict | pynput exception | Log warning, continue |
| X11 injection fail | python-xlib exception | Fall back to xdotool |
| xdotool fail | subprocess error | Fall back to clipboard + auto-paste |
| Transcription empty | Empty result | Skip output, no error beep |
| Recording too short | < 0.5s audio | Skip inference, single low beep |

**Graceful degradation for output:**
```
X11 injection → xdotool → clipboard + Ctrl+V paste
```

**Signal handling:**
- `SIGTERM` / `SIGINT`: Clean shutdown
- `SIGHUP`: Reload configuration

## File Structure

```
autowhisper/
├── src/
│   ├── autowhisper/          # Python package (new)
│   │   ├── __init__.py
│   │   ├── __main__.py
│   │   ├── config.py
│   │   ├── hotkey.py
│   │   ├── audio.py
│   │   ├── inference.py
│   │   ├── output.py
│   │   ├── feedback.py
│   │   └── daemon.py
├── config.toml
├── requirements.txt
├── pyproject.toml
├── autowhisper.service
└── install.sh
```

## Implementation Order

1. `config.py` - Foundation for everything
2. `inference.py` - Core value (faster-whisper)
3. `audio.py` - Recording + VAD
4. `feedback.py` - Simple, independent
5. `hotkey.py` - Event source
6. `output.py` - Text delivery
7. `daemon.py` - Ties everything together
8. `__main__.py` - Entry point + CLI
9. Update `install.sh` and `autowhisper.service`
10. Testing and benchmarking

## Success Criteria

- [ ] Inference latency < 150ms for 10s audio
- [ ] End-to-end latency < 250ms (hotkey release to text)
- [ ] No audio dropout during recording
- [ ] Stable for 8+ hours continuous operation
- [ ] Memory usage < 4GB VRAM

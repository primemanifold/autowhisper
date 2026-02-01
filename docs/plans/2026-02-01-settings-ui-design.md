# Settings UI Expansion Design

## Overview

Expand the `SettingsDialog` to expose all common TOML configuration options through a GTK UI with collapsible "Advanced" expanders per section.

## Current State

The settings dialog only exposes 4 settings:
- `audio.device` (microphone)
- `audio.output_device` (speaker)
- `hotkeys.trigger`
- `hotkeys.cancel`

## Target State

Expose all common settings organized into 5 sections, each with primary (always visible) and advanced (collapsible) settings.

## UI Layout

```
┌─────────────────────────────────────────────┐
│ AutoWhisper Settings                    [X] │
├─────────────────────────────────────────────┤
│  ┌─ Model ─────────────────────────────┐    │
│  │ Size: [distil-small.en      ▼]      │    │
│  │ ▸ Advanced                          │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  ┌─ Audio Devices ─────────────────────┐    │
│  │ Microphone: [System Default  ▼]     │    │
│  │ Speaker:    [System Default  ▼]     │    │
│  │ ▸ Advanced                          │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  ┌─ Hotkeys ───────────────────────────┐    │
│  │ Recording: [Shift+Super] [✕]        │    │
│  │ Cancel:    [Esc        ] [✕]        │    │
│  │ ▸ Advanced                          │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  ┌─ Output ────────────────────────────┐    │
│  │ Method: [inject ▼]                  │    │
│  │ ▸ Advanced                          │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  ┌─ Feedback ──────────────────────────┐    │
│  │ [✓] Enabled   Volume: [=====>    ]  │    │
│  │ ▸ Advanced                          │    │
│  └─────────────────────────────────────┘    │
│                                             │
│                    [ Cancel ]  [ Save ]     │
└─────────────────────────────────────────────┘
```

Dialog wrapped in `Gtk.ScrolledWindow` to handle expanded sections.

## Settings per Section

### Model Section
**Primary:**
- `size` - dropdown (tiny.en, base.en, small.en, distil-small.en, medium.en, distil-large-v3, large-v3, etc.)

**Advanced:**
- `device` - dropdown (cuda, cpu, auto)
- `compute_type` - dropdown (float16, bfloat16, int8, int8_float16, int8_float32, int8_bfloat16, float32)
- `beam_size` - spin button (1-5)
- `language` - dropdown (en, es, fr, auto, etc.)
- `num_threads` - spin button (1-16)

### Audio Devices Section
**Primary:**
- `device` - microphone dropdown (existing)
- `output_device` - speaker dropdown (existing)

**Advanced:**
- `vad_enabled` - checkbox
- `vad_threshold` - slider (0.0-1.0)
- `silence_duration` - spin button (0.1-1.0 seconds)
- `max_duration` - spin button (10-600 seconds)

### Hotkeys Section
**Primary:**
- `trigger` - HotkeyGroup widget (existing)
- `cancel` - HotkeyGroup widget (existing)

**Advanced:**
- `mode` - dropdown (push_to_talk, toggle)
- `escape_to_cancel` - checkbox

### Output Section
**Primary:**
- `method` - dropdown (inject, clipboard)

**Advanced:**
- `auto_paste` - checkbox (for clipboard mode)
- `paste_delay` - spin button (0.01-0.5 seconds)
- `append_newline` - checkbox
- `lowercase` - checkbox
- `also_copy_to_clipboard` - checkbox (for inject mode)

### Feedback Section
**Primary:**
- `enabled` - checkbox
- `volume` - slider (0.0-1.0)

**Advanced:**
- `frequency_start` - spin button (200-2000 Hz)
- `frequency_stop` - spin button (200-2000 Hz)
- `frequency_error` - spin button (200-2000 Hz)
- `duration` - spin button (0.05-0.5 seconds)

## Implementation

### Files to Modify
- `src/autowhisper/tray.py` - Expand `SettingsDialog` class

### SettingsDialog Changes
1. Add `config: Config` parameter to constructor (replace individual params)
2. Wrap content in `Gtk.ScrolledWindow`
3. Create helper methods:
   - `_build_model_section()` → `Gtk.Frame`
   - `_build_audio_section()` → `Gtk.Frame`
   - `_build_hotkeys_section()` → `Gtk.Frame`
   - `_build_output_section()` → `Gtk.Frame`
   - `_build_feedback_section()` → `Gtk.Frame`
4. Each section uses `Gtk.Expander` for advanced settings
5. Add properties: `model_config`, `audio_config`, `hotkeys_config`, `output_config`, `feedback_config`
6. Expand `_save_settings()` to persist all sections

### TrayManager Changes
- Pass full `Config` object to `SettingsDialog`
- Show "restart required" notification when model/audio settings change

### Save/Load Logic
```python
def _save_settings(self, config_values: dict) -> None:
    config = toml.load(config_path)

    # Merge each section
    config["model"] = {**config.get("model", {}), **config_values["model"]}
    config["audio"] = {**config.get("audio", {}), **config_values["audio"]}
    config["hotkeys"] = {**config.get("hotkeys", {}), **config_values["hotkeys"]}
    config["output"] = {**config.get("output", {}), **config_values["output"]}
    config["feedback"] = {**config.get("feedback", {}), **config_values["feedback"]}

    toml.dump(config, f)
```

### Restart Considerations
- Hotkeys, feedback settings: apply immediately
- Model, audio settings: require daemon restart
- Show notification: "Some changes require restart to take effect"

## Excluded Settings

Daemon and tray settings are not exposed in the UI (set once during installation):
- `daemon.log_level`, `daemon.log_file`, `daemon.pid_file`, `daemon.work_dir`
- `tray.enabled`
- `audio.sample_rate`, `audio.channels`, `audio.buffer_size` (fixed for Whisper)

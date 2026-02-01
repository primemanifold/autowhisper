# Settings UI Expansion Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Expand SettingsDialog to expose all common TOML config options with collapsible Advanced expanders per section.

**Architecture:** Refactor `SettingsDialog` to accept a `Config` object, build each section via helper methods, use `Gtk.Expander` for advanced settings, and wrap content in `Gtk.ScrolledWindow` for overflow.

**Tech Stack:** GTK3 (via PyGObject), TOML for config persistence

---

## Task 1: Update SettingsDialog Constructor Signature

**Files:**
- Modify: `src/autowhisper/tray.py:345-375`

**Step 1: Import Config at module level**

Add import near top of file (after line 9):

```python
from .config import Config, ModelConfig, AudioConfig, HotkeyConfig, OutputConfig, FeedbackConfig
```

**Step 2: Change constructor to accept Config object**

Replace the `__init__` signature (lines 348-354):

```python
    def __init__(self, config: Config):
        super().__init__(
            title="AutoWhisper Settings",
            flags=Gtk.DialogFlags.MODAL,
        )
        self.set_default_size(450, 600)

        self._config = config
```

**Step 3: Update audio device initialization**

Replace lines 365-374 with:

```python
        # Get available devices
        try:
            from .audio import list_audio_devices
            devices = list_audio_devices()
            self._input_devices = devices['input']
            self._output_devices = devices['output']
        except Exception as e:
            logger.warning(f"Could not list audio devices: {e}")
            self._input_devices = []
            self._output_devices = []
```

**Step 4: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "refactor(settings): accept Config object in SettingsDialog constructor"
```

---

## Task 2: Add ScrolledWindow and Restructure Content Area

**Files:**
- Modify: `src/autowhisper/tray.py:376-383`

**Step 1: Wrap content in ScrolledWindow**

Replace lines 376-382 with:

```python
        # Scrollable content area
        content_area = self.get_content_area()

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scrolled.set_min_content_height(400)
        content_area.pack_start(scrolled, True, True, 0)

        # Main container inside scroll
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_margin_top(12)
        box.set_margin_bottom(8)
        scrolled.add(box)
```

**Step 2: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "refactor(settings): wrap dialog content in ScrolledWindow"
```

---

## Task 3: Create Model Section Builder

**Files:**
- Modify: `src/autowhisper/tray.py` (add method after `_on_key_release`)

**Step 1: Add model size options constant**

Add after line 71 (after VERSION):

```python
MODEL_SIZES = [
    ("tiny.en", "Tiny (English) - Fastest"),
    ("base.en", "Base (English)"),
    ("small.en", "Small (English)"),
    ("distil-small.en", "Distil Small (English)"),
    ("medium.en", "Medium (English)"),
    ("distil-medium.en", "Distil Medium (English)"),
    ("distil-large-v3", "Distil Large v3 - Best"),
    ("large-v3", "Large v3"),
]

COMPUTE_TYPES = [
    ("float16", "Float16 (Default)"),
    ("bfloat16", "BFloat16 (RTX 50xx)"),
    ("int8", "Int8 (Smallest)"),
    ("int8_float16", "Int8+Float16"),
    ("float32", "Float32 (Slowest)"),
]

DEVICES = [
    ("cuda", "CUDA (GPU)"),
    ("cpu", "CPU"),
    ("auto", "Auto"),
]

LANGUAGES = [
    ("en", "English"),
    ("auto", "Auto-detect"),
    ("es", "Spanish"),
    ("fr", "French"),
    ("de", "German"),
    ("ja", "Japanese"),
    ("zh", "Chinese"),
]
```

**Step 2: Add _build_model_section method**

Add after the `_on_key_release` method (around line 527):

```python
    def _build_model_section(self) -> Gtk.Frame:
        """Build the Model settings section."""
        frame = Gtk.Frame()
        frame.set_label("  Model  ")
        frame.set_shadow_type(Gtk.ShadowType.ETCHED_IN)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.set_margin_start(12)
        vbox.set_margin_end(12)
        vbox.set_margin_top(8)
        vbox.set_margin_bottom(8)
        frame.add(vbox)

        # Primary: Model size
        size_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        size_label = Gtk.Label(label="Size:")
        size_label.set_xalign(0)
        size_label.set_size_request(90, -1)
        size_row.pack_start(size_label, False, False, 0)

        self._model_size_combo = Gtk.ComboBoxText()
        for model_id, display_name in MODEL_SIZES:
            self._model_size_combo.append(model_id, display_name)
        # Set active based on config
        active_idx = 0
        for i, (model_id, _) in enumerate(MODEL_SIZES):
            if model_id == self._config.model.size:
                active_idx = i
                break
        self._model_size_combo.set_active(active_idx)
        size_row.pack_start(self._model_size_combo, True, True, 0)
        vbox.pack_start(size_row, False, False, 0)

        # Advanced expander
        expander = Gtk.Expander(label="Advanced")
        adv_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        adv_box.set_margin_start(8)
        adv_box.set_margin_top(8)
        expander.add(adv_box)

        # Device
        device_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        device_label = Gtk.Label(label="Device:")
        device_label.set_xalign(0)
        device_label.set_size_request(100, -1)
        device_row.pack_start(device_label, False, False, 0)
        self._model_device_combo = Gtk.ComboBoxText()
        for dev_id, display_name in DEVICES:
            self._model_device_combo.append(dev_id, display_name)
        for i, (dev_id, _) in enumerate(DEVICES):
            if dev_id == self._config.model.device:
                self._model_device_combo.set_active(i)
                break
        device_row.pack_start(self._model_device_combo, True, True, 0)
        adv_box.pack_start(device_row, False, False, 0)

        # Compute type
        compute_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        compute_label = Gtk.Label(label="Compute:")
        compute_label.set_xalign(0)
        compute_label.set_size_request(100, -1)
        compute_row.pack_start(compute_label, False, False, 0)
        self._compute_type_combo = Gtk.ComboBoxText()
        for ct_id, display_name in COMPUTE_TYPES:
            self._compute_type_combo.append(ct_id, display_name)
        for i, (ct_id, _) in enumerate(COMPUTE_TYPES):
            if ct_id == self._config.model.compute_type:
                self._compute_type_combo.set_active(i)
                break
        compute_row.pack_start(self._compute_type_combo, True, True, 0)
        adv_box.pack_start(compute_row, False, False, 0)

        # Beam size
        beam_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        beam_label = Gtk.Label(label="Beam size:")
        beam_label.set_xalign(0)
        beam_label.set_size_request(100, -1)
        beam_row.pack_start(beam_label, False, False, 0)
        self._beam_size_spin = Gtk.SpinButton.new_with_range(1, 5, 1)
        self._beam_size_spin.set_value(self._config.model.beam_size)
        beam_row.pack_start(self._beam_size_spin, True, True, 0)
        adv_box.pack_start(beam_row, False, False, 0)

        # Language
        lang_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        lang_label = Gtk.Label(label="Language:")
        lang_label.set_xalign(0)
        lang_label.set_size_request(100, -1)
        lang_row.pack_start(lang_label, False, False, 0)
        self._language_combo = Gtk.ComboBoxText()
        for lang_id, display_name in LANGUAGES:
            self._language_combo.append(lang_id, display_name)
        active_lang = 0
        for i, (lang_id, _) in enumerate(LANGUAGES):
            if lang_id == self._config.model.language:
                active_lang = i
                break
        self._language_combo.set_active(active_lang)
        lang_row.pack_start(self._language_combo, True, True, 0)
        adv_box.pack_start(lang_row, False, False, 0)

        # CPU threads
        threads_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        threads_label = Gtk.Label(label="CPU threads:")
        threads_label.set_xalign(0)
        threads_label.set_size_request(100, -1)
        threads_row.pack_start(threads_label, False, False, 0)
        self._num_threads_spin = Gtk.SpinButton.new_with_range(1, 16, 1)
        self._num_threads_spin.set_value(self._config.model.num_threads)
        threads_row.pack_start(self._num_threads_spin, True, True, 0)
        adv_box.pack_start(threads_row, False, False, 0)

        vbox.pack_start(expander, False, False, 0)
        return frame
```

**Step 3: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): add Model section with advanced expander"
```

---

## Task 4: Create Audio Section Builder

**Files:**
- Modify: `src/autowhisper/tray.py`

**Step 1: Add _build_audio_section method**

Add after `_build_model_section`:

```python
    def _build_audio_section(self) -> Gtk.Frame:
        """Build the Audio Devices settings section."""
        frame = Gtk.Frame()
        frame.set_label("  Audio  ")
        frame.set_shadow_type(Gtk.ShadowType.ETCHED_IN)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.set_margin_start(12)
        vbox.set_margin_end(12)
        vbox.set_margin_top(8)
        vbox.set_margin_bottom(8)
        frame.add(vbox)

        # Primary: Microphone selector
        mic_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        mic_label = Gtk.Label(label="Microphone:")
        mic_label.set_xalign(0)
        mic_label.set_size_request(90, -1)
        mic_row.pack_start(mic_label, False, False, 0)

        self._mic_combo = Gtk.ComboBoxText()
        self._mic_combo.append("default", "System Default")
        active_input = 0
        for i, (idx, name) in enumerate(self._input_devices):
            self._mic_combo.append(str(idx), self._truncate_name(name, 35))
            if self._config.audio.device and (str(idx) == str(self._config.audio.device) or name == self._config.audio.device):
                active_input = i + 1
        self._mic_combo.set_active(active_input)
        mic_row.pack_start(self._mic_combo, True, True, 0)
        vbox.pack_start(mic_row, False, False, 0)

        # Primary: Speaker selector
        spk_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        spk_label = Gtk.Label(label="Speaker:")
        spk_label.set_xalign(0)
        spk_label.set_size_request(90, -1)
        spk_row.pack_start(spk_label, False, False, 0)

        self._spk_combo = Gtk.ComboBoxText()
        self._spk_combo.append("default", "System Default")
        active_output = 0
        for i, (idx, name) in enumerate(self._output_devices):
            self._spk_combo.append(str(idx), self._truncate_name(name, 35))
            if self._config.audio.output_device and (str(idx) == str(self._config.audio.output_device) or name == self._config.audio.output_device):
                active_output = i + 1
        self._spk_combo.set_active(active_output)
        spk_row.pack_start(self._spk_combo, True, True, 0)
        vbox.pack_start(spk_row, False, False, 0)

        # Advanced expander
        expander = Gtk.Expander(label="Advanced")
        adv_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        adv_box.set_margin_start(8)
        adv_box.set_margin_top(8)
        expander.add(adv_box)

        # VAD enabled
        self._vad_enabled_check = Gtk.CheckButton(label="Voice Activity Detection (VAD)")
        self._vad_enabled_check.set_active(self._config.audio.vad_enabled)
        adv_box.pack_start(self._vad_enabled_check, False, False, 0)

        # VAD threshold
        vad_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        vad_label = Gtk.Label(label="VAD threshold:")
        vad_label.set_xalign(0)
        vad_label.set_size_request(110, -1)
        vad_row.pack_start(vad_label, False, False, 0)
        self._vad_threshold_scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0.0, 1.0, 0.1)
        self._vad_threshold_scale.set_value(self._config.audio.vad_threshold)
        self._vad_threshold_scale.set_digits(1)
        vad_row.pack_start(self._vad_threshold_scale, True, True, 0)
        adv_box.pack_start(vad_row, False, False, 0)

        # Silence duration
        silence_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        silence_label = Gtk.Label(label="Silence trim (s):")
        silence_label.set_xalign(0)
        silence_label.set_size_request(110, -1)
        silence_row.pack_start(silence_label, False, False, 0)
        self._silence_duration_spin = Gtk.SpinButton.new_with_range(0.1, 2.0, 0.1)
        self._silence_duration_spin.set_value(self._config.audio.silence_duration)
        self._silence_duration_spin.set_digits(1)
        silence_row.pack_start(self._silence_duration_spin, True, True, 0)
        adv_box.pack_start(silence_row, False, False, 0)

        # Max duration
        max_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        max_label = Gtk.Label(label="Max duration (s):")
        max_label.set_xalign(0)
        max_label.set_size_request(110, -1)
        max_row.pack_start(max_label, False, False, 0)
        self._max_duration_spin = Gtk.SpinButton.new_with_range(10, 600, 10)
        self._max_duration_spin.set_value(self._config.audio.max_duration)
        max_row.pack_start(self._max_duration_spin, True, True, 0)
        adv_box.pack_start(max_row, False, False, 0)

        vbox.pack_start(expander, False, False, 0)
        return frame
```

**Step 2: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): add Audio section with advanced expander"
```

---

## Task 5: Create Hotkeys Section Builder

**Files:**
- Modify: `src/autowhisper/tray.py`

**Step 1: Add _build_hotkeys_section method**

Add after `_build_audio_section`:

```python
    def _build_hotkeys_section(self) -> Gtk.Frame:
        """Build the Hotkeys settings section."""
        frame = Gtk.Frame()
        frame.set_label("  Hotkeys  ")
        frame.set_shadow_type(Gtk.ShadowType.ETCHED_IN)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.set_margin_start(12)
        vbox.set_margin_end(12)
        vbox.set_margin_top(8)
        vbox.set_margin_bottom(8)
        frame.add(vbox)

        # Primary: Trigger hotkeys
        self._trigger_group = HotkeyGroup(
            "Recording (hold to speak)",
            self._config.hotkeys.trigger,
            "shift+super"
        )
        vbox.pack_start(self._trigger_group, False, False, 0)

        # Primary: Cancel hotkeys
        self._cancel_group = HotkeyGroup(
            "Cancel Recording",
            self._config.hotkeys.cancel,
            "esc"
        )
        vbox.pack_start(self._cancel_group, False, False, 0)

        # Hint
        hint = Gtk.Label()
        hint.set_markup("<small>Click button, press keys. Backspace clears.</small>")
        hint.set_opacity(0.6)
        vbox.pack_start(hint, False, False, 0)

        # Advanced expander
        expander = Gtk.Expander(label="Advanced")
        adv_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        adv_box.set_margin_start(8)
        adv_box.set_margin_top(8)
        expander.add(adv_box)

        # Mode
        mode_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        mode_label = Gtk.Label(label="Mode:")
        mode_label.set_xalign(0)
        mode_label.set_size_request(110, -1)
        mode_row.pack_start(mode_label, False, False, 0)
        self._hotkey_mode_combo = Gtk.ComboBoxText()
        self._hotkey_mode_combo.append("push_to_talk", "Push to Talk (hold)")
        self._hotkey_mode_combo.append("toggle", "Toggle (press twice)")
        self._hotkey_mode_combo.set_active(0 if self._config.hotkeys.mode == "push_to_talk" else 1)
        mode_row.pack_start(self._hotkey_mode_combo, True, True, 0)
        adv_box.pack_start(mode_row, False, False, 0)

        # Escape to cancel
        self._escape_to_cancel_check = Gtk.CheckButton(label="Escape key cancels recording")
        self._escape_to_cancel_check.set_active(self._config.hotkeys.escape_to_cancel)
        adv_box.pack_start(self._escape_to_cancel_check, False, False, 0)

        vbox.pack_start(expander, False, False, 0)
        return frame
```

**Step 2: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): add Hotkeys section with advanced expander"
```

---

## Task 6: Create Output Section Builder

**Files:**
- Modify: `src/autowhisper/tray.py`

**Step 1: Add _build_output_section method**

Add after `_build_hotkeys_section`:

```python
    def _build_output_section(self) -> Gtk.Frame:
        """Build the Output settings section."""
        frame = Gtk.Frame()
        frame.set_label("  Output  ")
        frame.set_shadow_type(Gtk.ShadowType.ETCHED_IN)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.set_margin_start(12)
        vbox.set_margin_end(12)
        vbox.set_margin_top(8)
        vbox.set_margin_bottom(8)
        frame.add(vbox)

        # Primary: Method
        method_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        method_label = Gtk.Label(label="Method:")
        method_label.set_xalign(0)
        method_label.set_size_request(90, -1)
        method_row.pack_start(method_label, False, False, 0)
        self._output_method_combo = Gtk.ComboBoxText()
        self._output_method_combo.append("inject", "Type text (xdotool)")
        self._output_method_combo.append("clipboard", "Copy to clipboard")
        self._output_method_combo.set_active(0 if self._config.output.method == "inject" else 1)
        method_row.pack_start(self._output_method_combo, True, True, 0)
        vbox.pack_start(method_row, False, False, 0)

        # Advanced expander
        expander = Gtk.Expander(label="Advanced")
        adv_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        adv_box.set_margin_start(8)
        adv_box.set_margin_top(8)
        expander.add(adv_box)

        # Also copy to clipboard (for inject mode)
        self._also_copy_check = Gtk.CheckButton(label="Also copy to clipboard (inject mode)")
        self._also_copy_check.set_active(self._config.output.also_copy_to_clipboard)
        adv_box.pack_start(self._also_copy_check, False, False, 0)

        # Auto paste (for clipboard mode)
        self._auto_paste_check = Gtk.CheckButton(label="Auto-paste after copy (clipboard mode)")
        self._auto_paste_check.set_active(self._config.output.auto_paste)
        adv_box.pack_start(self._auto_paste_check, False, False, 0)

        # Paste delay
        delay_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        delay_label = Gtk.Label(label="Paste delay (s):")
        delay_label.set_xalign(0)
        delay_label.set_size_request(110, -1)
        delay_row.pack_start(delay_label, False, False, 0)
        self._paste_delay_spin = Gtk.SpinButton.new_with_range(0.01, 0.5, 0.01)
        self._paste_delay_spin.set_value(self._config.output.paste_delay)
        self._paste_delay_spin.set_digits(2)
        delay_row.pack_start(self._paste_delay_spin, True, True, 0)
        adv_box.pack_start(delay_row, False, False, 0)

        # Append newline
        self._append_newline_check = Gtk.CheckButton(label="Append newline after text")
        self._append_newline_check.set_active(self._config.output.append_newline)
        adv_box.pack_start(self._append_newline_check, False, False, 0)

        # Lowercase
        self._lowercase_check = Gtk.CheckButton(label="Convert to lowercase")
        self._lowercase_check.set_active(self._config.output.lowercase)
        adv_box.pack_start(self._lowercase_check, False, False, 0)

        vbox.pack_start(expander, False, False, 0)
        return frame
```

**Step 2: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): add Output section with advanced expander"
```

---

## Task 7: Create Feedback Section Builder

**Files:**
- Modify: `src/autowhisper/tray.py`

**Step 1: Add _build_feedback_section method**

Add after `_build_output_section`:

```python
    def _build_feedback_section(self) -> Gtk.Frame:
        """Build the Feedback settings section."""
        frame = Gtk.Frame()
        frame.set_label("  Feedback  ")
        frame.set_shadow_type(Gtk.ShadowType.ETCHED_IN)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.set_margin_start(12)
        vbox.set_margin_end(12)
        vbox.set_margin_top(8)
        vbox.set_margin_bottom(8)
        frame.add(vbox)

        # Primary: Enabled + Volume row
        primary_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)

        self._feedback_enabled_check = Gtk.CheckButton(label="Enabled")
        self._feedback_enabled_check.set_active(self._config.feedback.enabled)
        primary_row.pack_start(self._feedback_enabled_check, False, False, 0)

        vol_label = Gtk.Label(label="Volume:")
        vol_label.set_xalign(0)
        primary_row.pack_start(vol_label, False, False, 0)

        self._feedback_volume_scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0.0, 1.0, 0.1)
        self._feedback_volume_scale.set_value(self._config.feedback.volume)
        self._feedback_volume_scale.set_digits(1)
        self._feedback_volume_scale.set_size_request(120, -1)
        primary_row.pack_start(self._feedback_volume_scale, True, True, 0)

        vbox.pack_start(primary_row, False, False, 0)

        # Advanced expander
        expander = Gtk.Expander(label="Advanced")
        adv_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        adv_box.set_margin_start(8)
        adv_box.set_margin_top(8)
        expander.add(adv_box)

        # Start frequency
        start_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        start_label = Gtk.Label(label="Start freq (Hz):")
        start_label.set_xalign(0)
        start_label.set_size_request(110, -1)
        start_row.pack_start(start_label, False, False, 0)
        self._freq_start_spin = Gtk.SpinButton.new_with_range(200, 2000, 50)
        self._freq_start_spin.set_value(self._config.feedback.frequency_start)
        start_row.pack_start(self._freq_start_spin, True, True, 0)
        adv_box.pack_start(start_row, False, False, 0)

        # Stop frequency
        stop_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        stop_label = Gtk.Label(label="Stop freq (Hz):")
        stop_label.set_xalign(0)
        stop_label.set_size_request(110, -1)
        stop_row.pack_start(stop_label, False, False, 0)
        self._freq_stop_spin = Gtk.SpinButton.new_with_range(200, 2000, 50)
        self._freq_stop_spin.set_value(self._config.feedback.frequency_stop)
        stop_row.pack_start(self._freq_stop_spin, True, True, 0)
        adv_box.pack_start(stop_row, False, False, 0)

        # Error frequency
        error_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        error_label = Gtk.Label(label="Error freq (Hz):")
        error_label.set_xalign(0)
        error_label.set_size_request(110, -1)
        error_row.pack_start(error_label, False, False, 0)
        self._freq_error_spin = Gtk.SpinButton.new_with_range(200, 2000, 50)
        self._freq_error_spin.set_value(self._config.feedback.frequency_error)
        error_row.pack_start(self._freq_error_spin, True, True, 0)
        adv_box.pack_start(error_row, False, False, 0)

        # Duration
        dur_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        dur_label = Gtk.Label(label="Duration (s):")
        dur_label.set_xalign(0)
        dur_label.set_size_request(110, -1)
        dur_row.pack_start(dur_label, False, False, 0)
        self._feedback_duration_spin = Gtk.SpinButton.new_with_range(0.05, 0.5, 0.05)
        self._feedback_duration_spin.set_value(self._config.feedback.duration)
        self._feedback_duration_spin.set_digits(2)
        dur_row.pack_start(self._feedback_duration_spin, True, True, 0)
        adv_box.pack_start(dur_row, False, False, 0)

        vbox.pack_start(expander, False, False, 0)
        return frame
```

**Step 2: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): add Feedback section with advanced expander"
```

---

## Task 8: Wire Up Sections in Constructor

**Files:**
- Modify: `src/autowhisper/tray.py:376-463`

**Step 1: Replace inline section building with helper calls**

Replace the section building code (after ScrolledWindow setup) with:

```python
        # Build sections
        box.pack_start(self._build_model_section(), False, False, 0)
        box.pack_start(self._build_audio_section(), False, False, 0)
        box.pack_start(self._build_hotkeys_section(), False, False, 0)
        box.pack_start(self._build_output_section(), False, False, 0)
        box.pack_start(self._build_feedback_section(), False, False, 0)

        # Key capture events
        self.connect("key-press-event", self._on_key_press)
        self.connect("key-release-event", self._on_key_release)

        self.show_all()
```

**Step 2: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "refactor(settings): wire up all section builders in constructor"
```

---

## Task 9: Add Config Property Getters

**Files:**
- Modify: `src/autowhisper/tray.py`

**Step 1: Add properties to collect settings values**

Add after existing properties (around line 505):

```python
    @property
    def model_config(self) -> dict:
        """Get model settings as dict."""
        return {
            "size": self._model_size_combo.get_active_id(),
            "device": self._model_device_combo.get_active_id(),
            "compute_type": self._compute_type_combo.get_active_id(),
            "beam_size": int(self._beam_size_spin.get_value()),
            "language": self._language_combo.get_active_id(),
            "num_threads": int(self._num_threads_spin.get_value()),
        }

    @property
    def audio_config(self) -> dict:
        """Get audio settings as dict."""
        input_dev = self._mic_combo.get_active_id()
        output_dev = self._spk_combo.get_active_id()
        return {
            "device": None if input_dev == "default" else input_dev,
            "output_device": None if output_dev == "default" else output_dev,
            "vad_enabled": self._vad_enabled_check.get_active(),
            "vad_threshold": self._vad_threshold_scale.get_value(),
            "silence_duration": self._silence_duration_spin.get_value(),
            "max_duration": self._max_duration_spin.get_value(),
        }

    @property
    def hotkeys_config(self) -> dict:
        """Get hotkey settings as dict."""
        return {
            "trigger": self._trigger_group.hotkeys,
            "cancel": self._cancel_group.hotkeys,
            "mode": self._hotkey_mode_combo.get_active_id(),
            "escape_to_cancel": self._escape_to_cancel_check.get_active(),
        }

    @property
    def output_config(self) -> dict:
        """Get output settings as dict."""
        return {
            "method": self._output_method_combo.get_active_id(),
            "also_copy_to_clipboard": self._also_copy_check.get_active(),
            "auto_paste": self._auto_paste_check.get_active(),
            "paste_delay": self._paste_delay_spin.get_value(),
            "append_newline": self._append_newline_check.get_active(),
            "lowercase": self._lowercase_check.get_active(),
        }

    @property
    def feedback_config(self) -> dict:
        """Get feedback settings as dict."""
        return {
            "enabled": self._feedback_enabled_check.get_active(),
            "volume": self._feedback_volume_scale.get_value(),
            "frequency_start": int(self._freq_start_spin.get_value()),
            "frequency_stop": int(self._freq_stop_spin.get_value()),
            "frequency_error": int(self._freq_error_spin.get_value()),
            "duration": self._feedback_duration_spin.get_value(),
        }
```

**Step 2: Keep backwards-compatible properties**

The existing `trigger_hotkeys`, `cancel_hotkeys`, `input_device`, `output_device` properties should continue to work (they reference the same widgets).

**Step 3: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): add config property getters for all sections"
```

---

## Task 10: Update TrayManager to Pass Config

**Files:**
- Modify: `src/autowhisper/tray.py:652-714`

**Step 1: Add config storage to TrayManager**

In `TrayManager.__init__` (around line 543), add:

```python
        self._config: Config | None = None
```

**Step 2: Add method to set config**

Add to TrayManager class:

```python
    def set_config(self, config: Config) -> None:
        """Set the config object for settings dialog."""
        self._config = config
```

**Step 3: Update _show_settings_dialog**

Replace the dialog creation (line 664-669) with:

```python
        if not self._config:
            logger.error("No config available for settings dialog")
            if self._on_settings_close:
                self._on_settings_close()
            return False

        # Show settings dialog with config
        dialog = SettingsDialog(self._config)
        response = dialog.run()
```

**Step 4: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "refactor(tray): pass Config object to SettingsDialog"
```

---

## Task 11: Update Save Settings Logic

**Files:**
- Modify: `src/autowhisper/tray.py:722-763`

**Step 1: Expand _save_settings to handle all sections**

Replace `_save_settings` method:

```python
    def _save_settings(self, dialog: SettingsDialog) -> None:
        """Save all settings to config file."""
        if not self._config_path:
            return

        try:
            import toml
            config_path = Path(self._config_path)
            if config_path.exists():
                config = toml.load(config_path)
            else:
                config = {}

            # Model settings
            model_cfg = dialog.model_config
            if "model" not in config:
                config["model"] = {}
            config["model"].update(model_cfg)

            # Audio settings
            audio_cfg = dialog.audio_config
            if "audio" not in config:
                config["audio"] = {}
            # Handle None values (remove from config)
            for key, value in audio_cfg.items():
                if value is None:
                    config["audio"].pop(key, None)
                else:
                    config["audio"][key] = value

            # Hotkey settings
            hotkeys_cfg = dialog.hotkeys_config
            if "hotkeys" not in config:
                config["hotkeys"] = {}
            config["hotkeys"].update(hotkeys_cfg)

            # Output settings
            output_cfg = dialog.output_config
            if "output" not in config:
                config["output"] = {}
            config["output"].update(output_cfg)

            # Feedback settings
            feedback_cfg = dialog.feedback_config
            if "feedback" not in config:
                config["feedback"] = {}
            config["feedback"].update(feedback_cfg)

            with open(config_path, "w") as f:
                toml.dump(config, f)
            logger.info(f"Saved settings to {config_path}")
        except Exception as e:
            logger.error(f"Failed to save settings: {e}")
```

**Step 2: Update dialog response handling**

Update `_show_settings_dialog` to use new save method (around line 672):

```python
        if response == Gtk.ResponseType.OK:
            # Check what changed for restart notification
            model_changed = (
                dialog.model_config["size"] != self._config.model.size or
                dialog.model_config["device"] != self._config.model.device or
                dialog.model_config["compute_type"] != self._config.model.compute_type
            )

            # Save all settings
            self._save_settings(dialog)

            # Update local state for menu display
            self._trigger_hotkeys = dialog.trigger_hotkeys
            self._cancel_hotkeys = dialog.cancel_hotkeys
            self._input_device_id = dialog.input_device
            self._output_device_id = dialog.output_device
            self._input_device = dialog.input_device_name
            self._output_device = dialog.output_device_name

            # Update menu labels
            self._update_trigger_label()
            self._update_cancel_label()
            self._update_mic_label(self._input_device)
            self._update_speaker_label(self._output_device)

            if model_changed:
                logger.info("Model settings changed - restart required")
```

**Step 3: Commit**

```bash
git add src/autowhisper/tray.py
git commit -m "feat(settings): expand save logic for all config sections"
```

---

## Task 12: Update Daemon to Pass Config to TrayManager

**Files:**
- Modify: `src/autowhisper/daemon.py`

**Step 1: Find where TrayManager is created and add set_config call**

After TrayManager is initialized, add:

```python
        if self._tray:
            self._tray.set_config(self._config)
```

**Step 2: Commit**

```bash
git add src/autowhisper/daemon.py
git commit -m "feat(daemon): pass config to TrayManager for settings dialog"
```

---

## Task 13: Manual Testing

**Step 1: Run the daemon**

```bash
cd /home/isura/autowhisper
python -m autowhisper.daemon
```

**Step 2: Open Settings from tray**

Click tray icon → Settings...

**Step 3: Verify all sections render**

- [ ] Model section with size dropdown and Advanced expander
- [ ] Audio section with mic/speaker dropdowns and Advanced expander
- [ ] Hotkeys section with trigger/cancel and Advanced expander
- [ ] Output section with method dropdown and Advanced expander
- [ ] Feedback section with enabled/volume and Advanced expander

**Step 4: Verify expanders work**

- [ ] Click each "Advanced" expander to expand/collapse
- [ ] Dialog scrolls when multiple expanders are open

**Step 5: Verify save works**

- [ ] Change a setting in each section
- [ ] Click Save
- [ ] Check config.toml reflects changes

**Step 6: Commit final state**

```bash
git add -A
git commit -m "feat(settings): complete settings UI expansion with all config sections"
```

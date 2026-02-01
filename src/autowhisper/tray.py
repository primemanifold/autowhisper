"""System tray icon for AutoWhisper using AppIndicator3."""

from __future__ import annotations

import logging
import threading
from enum import Enum, auto
from pathlib import Path

from .config import Config, ModelConfig, AudioConfig, HotkeyConfig, OutputConfig, FeedbackConfig

logger = logging.getLogger(__name__)

# Icon directory (relative to this file)
ICONS_DIR = Path(__file__).parent / "icons"

# Try to import AppIndicator (prefer Ayatana, fall back to legacy)
TRAY_AVAILABLE = False
AppIndicator = None
Gtk = None

try:
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, GLib

    # Try Ayatana first (newer), then legacy AppIndicator
    try:
        gi.require_version('AyatanaAppIndicator3', '0.1')
        from gi.repository import AyatanaAppIndicator3 as AppIndicator
        TRAY_AVAILABLE = True
        logger.debug("Using AyatanaAppIndicator3")
    except (ValueError, ImportError):
        try:
            gi.require_version('AppIndicator3', '0.1')
            from gi.repository import AppIndicator3 as AppIndicator
            TRAY_AVAILABLE = True
            logger.debug("Using AppIndicator3")
        except (ValueError, ImportError):
            pass
except ImportError:
    pass

if not TRAY_AVAILABLE:
    logger.debug("AppIndicator not available")


class TrayState(Enum):
    """Tray icon states."""
    IDLE = auto()
    RECORDING = auto()
    PROCESSING = auto()
    ERROR = auto()


# Map states to icon filenames (without extension)
ICON_NAMES = {
    TrayState.IDLE: "idle",
    TrayState.RECORDING: "recording",
    TrayState.PROCESSING: "processing",
    TrayState.ERROR: "error",
}

# Tooltip titles for each state
STATE_TITLES = {
    TrayState.IDLE: "AutoWhisper - Ready",
    TrayState.RECORDING: "AutoWhisper - Recording...",
    TrayState.PROCESSING: "AutoWhisper - Transcribing...",
    TrayState.ERROR: "AutoWhisper - Error",
}


VERSION = "0.1.0"

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


def normalize_key(keyname: str) -> str | None:
    """Normalize key name to match pynput format."""
    key_map = {
        # Modifiers
        "Shift_L": "shift", "Shift_R": "shift",
        "Control_L": "ctrl", "Control_R": "ctrl",
        "Alt_L": "alt", "Alt_R": "alt",
        "Super_L": "super", "Super_R": "super",
        "Meta_L": "super", "Meta_R": "super",
        # Common keys
        "Escape": "esc",
        "Return": "enter",
        "space": "space",
        "Tab": "tab",
        # Media keys (XF86Audio* from GTK -> pynput names)
        "XF86AudioPlay": "media_play_pause",
        "XF86AudioPause": "media_play_pause",
        "XF86AudioStop": "media_play_pause",
        "XF86AudioMute": "media_volume_mute",
        "XF86AudioMicMute": "media_volume_mute",  # Treat mic mute same as mute
        "XF86AudioNext": "media_next",
        "XF86AudioPrev": "media_previous",
        "XF86AudioRaiseVolume": "media_volume_up",
        "XF86AudioLowerVolume": "media_volume_down",
    }

    if keyname in key_map:
        return key_map[keyname]

    if len(keyname) == 1:
        return keyname.lower()

    if keyname.startswith("F") and keyname[1:].isdigit():
        return keyname.lower()

    return None


def display_key(keyname: str) -> str:
    """Convert internal key name to user-friendly display name."""
    display_map = {
        # Media keys - show friendly names
        "media_play_pause": "Play/Pause",
        "media_volume_mute": "Mute",
        "media_volume_up": "Vol+",
        "media_volume_down": "Vol-",
        "media_next": "Next",
        "media_previous": "Prev",
        # Common keys
        "esc": "Esc",
        "enter": "Enter",
        "space": "Space",
        "tab": "Tab",
        # Modifiers
        "shift": "Shift",
        "ctrl": "Ctrl",
        "alt": "Alt",
        "super": "Super",
    }
    return display_map.get(keyname, keyname.capitalize() if len(keyname) > 1 else keyname.upper())


def display_hotkey(hotkey: str) -> str:
    """Convert hotkey string to user-friendly display format."""
    if not hotkey:
        return ""
    parts = hotkey.split("+")
    return "+".join(display_key(p) for p in parts)


class HotkeyRow(Gtk.Box):
    """A single hotkey entry row with capture button and remove button."""

    def __init__(self, hotkey: str, on_remove: callable, removable: bool = True):
        super().__init__(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)

        self.hotkey = hotkey
        self._on_remove = on_remove
        self._keys_pressed = set()
        self._capturing = False

        # Shortcut button
        self._shortcut_btn = Gtk.Button()
        self._shortcut_btn.set_size_request(180, 36)
        self._update_button_label()
        self._shortcut_btn.connect("clicked", self._start_capture)
        self.pack_start(self._shortcut_btn, True, True, 0)

        # Remove button
        self._remove_btn = Gtk.Button(label="✕")
        self._remove_btn.set_size_request(36, 36)
        self._remove_btn.set_sensitive(removable)
        self._remove_btn.connect("clicked", self._on_remove_clicked)
        self.pack_start(self._remove_btn, False, False, 0)

    def set_removable(self, removable: bool) -> None:
        """Enable/disable the remove button."""
        self._remove_btn.set_sensitive(removable)

    def _update_button_label(self) -> None:
        """Update the shortcut button label."""
        if self._capturing:
            self._shortcut_btn.set_label("Press keys...")
            self._shortcut_btn.get_style_context().add_class("suggested-action")
        elif self.hotkey:
            self._shortcut_btn.set_label(display_hotkey(self.hotkey))
            self._shortcut_btn.get_style_context().remove_class("suggested-action")
        else:
            self._shortcut_btn.set_label("(click to set)")
            self._shortcut_btn.get_style_context().remove_class("suggested-action")

    def _start_capture(self, widget) -> None:
        """Start capturing key presses."""
        self._capturing = True
        self._keys_pressed = set()
        self._update_button_label()

    def stop_capture(self) -> None:
        """Stop capturing and save result."""
        if not self._capturing:
            return
        self._capturing = False
        if self._keys_pressed:
            self.hotkey = "+".join(sorted(self._keys_pressed))
        self._update_button_label()

    def handle_key_press(self, keyname: str) -> bool:
        """Handle key press. Returns True if handled."""
        if not self._capturing:
            return False

        # Escape cancels capture
        if keyname == "Escape":
            self._capturing = False
            self._keys_pressed = set()
            self._update_button_label()
            return True

        # Backspace clears
        if keyname == "BackSpace":
            self.hotkey = None
            self._keys_pressed = set()
            self.stop_capture()
            return True

        # Normalize and add key
        normalized = normalize_key(keyname)
        if normalized:
            self._keys_pressed.add(normalized)
            display = "+".join(display_key(k) for k in sorted(self._keys_pressed))
            self._shortcut_btn.set_label(display + "...")

        return True

    def handle_key_release(self) -> bool:
        """Handle key release. Returns True if handled."""
        if self._capturing and self._keys_pressed:
            self.stop_capture()
            return True
        return False

    @property
    def is_capturing(self) -> bool:
        return self._capturing

    def _on_remove_clicked(self, widget) -> None:
        """Handle remove button click."""
        if self._on_remove:
            self._on_remove(self)


class HotkeyGroup(Gtk.Frame):
    """A group widget for configuring multiple hotkeys for one function."""

    def __init__(self, title: str, current_hotkeys: list[str], default_hotkey: str):
        super().__init__()
        self.set_label(f"  {title}  ")
        self.set_shadow_type(Gtk.ShadowType.ETCHED_IN)

        self._default = default_hotkey
        self._rows: list[HotkeyRow] = []

        # Content box
        self._box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self._box.set_margin_start(12)
        self._box.set_margin_end(12)
        self._box.set_margin_top(8)
        self._box.set_margin_bottom(8)
        self.add(self._box)

        # Container for hotkey rows
        self._rows_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        self._box.pack_start(self._rows_box, False, False, 0)

        # Add existing hotkeys
        hotkeys = current_hotkeys if current_hotkeys else [default_hotkey]
        for hk in hotkeys:
            self._add_row(hk)

        # Button row
        btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        btn_box.set_halign(Gtk.Align.CENTER)

        add_btn = Gtk.Button(label="+ Add")
        add_btn.connect("clicked", self._on_add)
        btn_box.pack_start(add_btn, False, False, 0)

        reset_btn = Gtk.Button(label="Reset")
        reset_btn.connect("clicked", self._on_reset)
        btn_box.pack_start(reset_btn, False, False, 0)

        self._box.pack_start(btn_box, False, False, 4)

    def _add_row(self, hotkey: str) -> HotkeyRow:
        """Add a new hotkey row."""
        row = HotkeyRow(hotkey, self._remove_row, removable=len(self._rows) > 0)
        self._rows.append(row)
        self._rows_box.pack_start(row, False, False, 0)
        row.show_all()
        self._update_remove_buttons()
        return row

    def _remove_row(self, row: HotkeyRow) -> None:
        """Remove a hotkey row."""
        if row in self._rows and len(self._rows) > 1:
            self._rows.remove(row)
            self._rows_box.remove(row)
            self._update_remove_buttons()

    def _update_remove_buttons(self) -> None:
        """Update remove button states - can't remove last one."""
        for i, row in enumerate(self._rows):
            row.set_removable(len(self._rows) > 1)

    def _on_add(self, widget) -> None:
        """Add a new empty hotkey slot."""
        self._add_row(None)

    def _on_reset(self, widget) -> None:
        """Reset to single default hotkey."""
        # Remove all rows
        for row in self._rows[:]:
            self._rows_box.remove(row)
        self._rows.clear()
        # Add default
        self._add_row(self._default)

    @property
    def hotkeys(self) -> list[str]:
        """Get list of configured hotkeys (non-empty only)."""
        return [r.hotkey for r in self._rows if r.hotkey]

    def handle_key_press(self, keyname: str) -> bool:
        """Route key press to capturing row."""
        for row in self._rows:
            if row.handle_key_press(keyname):
                return True
        return False

    def handle_key_release(self) -> bool:
        """Route key release to capturing row."""
        for row in self._rows:
            if row.handle_key_release():
                return True
        return False

    @property
    def is_capturing(self) -> bool:
        return any(r.is_capturing for r in self._rows)


class SettingsDialog(Gtk.Dialog):
    """Dialog for configuring AutoWhisper settings."""

    def __init__(self, config: Config):
        super().__init__(
            title="AutoWhisper Settings",
            flags=Gtk.DialogFlags.MODAL,
        )
        self.set_default_size(450, 600)

        self._config = config

        # Add buttons
        self.add_button("Cancel", Gtk.ResponseType.CANCEL)
        self.add_button("Save", Gtk.ResponseType.OK)

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

    def _truncate_name(self, name: str, max_len: int) -> str:
        """Truncate device name for display."""
        if len(name) <= max_len:
            return name
        return name[:max_len - 3] + "..."

    @property
    def trigger_hotkeys(self) -> list[str]:
        return self._trigger_group.hotkeys

    @property
    def cancel_hotkeys(self) -> list[str]:
        return self._cancel_group.hotkeys

    @property
    def input_device(self) -> str | None:
        """Get selected input device (None for default)."""
        active_id = self._mic_combo.get_active_id()
        return None if active_id == "default" else active_id

    @property
    def output_device(self) -> str | None:
        """Get selected output device (None for default)."""
        active_id = self._spk_combo.get_active_id()
        return None if active_id == "default" else active_id

    @property
    def input_device_name(self) -> str:
        """Get selected input device name for display."""
        active = self._mic_combo.get_active()
        if active == 0:
            return "Default"
        return self._mic_combo.get_active_text()

    @property
    def output_device_name(self) -> str:
        """Get selected output device name for display."""
        active = self._spk_combo.get_active()
        if active == 0:
            return "Default"
        return self._spk_combo.get_active_text()

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

    def _on_key_press(self, widget, event) -> bool:
        """Route key press to active group."""
        from gi.repository import Gdk
        keyname = Gdk.keyval_name(event.keyval)
        if not keyname:
            return False

        # Try trigger group first, then cancel group
        if self._trigger_group.handle_key_press(keyname):
            return True
        if self._cancel_group.handle_key_press(keyname):
            return True
        return False

    def _on_key_release(self, widget, event) -> bool:
        """Route key release to active group."""
        if self._trigger_group.handle_key_release():
            return True
        if self._cancel_group.handle_key_release():
            return True
        return False

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


# Keep old name for compatibility
HotkeySettingsDialog = SettingsDialog


class TrayManager:
    """Manages the system tray icon using AppIndicator3."""

    def __init__(
        self,
        enabled: bool = True,
        on_quit: callable = None,
        on_settings_open: callable = None,
        on_settings_close: callable = None,
        config_path: str = None,
    ):
        self._enabled = enabled and TRAY_AVAILABLE
        self._indicator = None
        self._state = TrayState.IDLE
        self._thread: threading.Thread | None = None
        self._gtk_loop = None
        self._on_quit = on_quit
        self._on_settings_open = on_settings_open
        self._on_settings_close = on_settings_close
        self._config_path = config_path
        self._config: Config | None = None
        self._mic_label = None
        self._speaker_label = None
        self._trigger_label = None
        self._cancel_label = None
        self._input_device = "Default"
        self._output_device = "Default"
        self._input_device_id = None  # Config device ID
        self._output_device_id = None  # Config device ID
        self._trigger_hotkeys = ["shift+super"]
        self._cancel_hotkeys = ["esc"]

        if not TRAY_AVAILABLE and enabled:
            logger.warning(
                "Tray icon disabled: AppIndicator3 not available. "
                "Install with: sudo apt install gir1.2-ayatanaappindicator3-0.1"
            )

    def set_config(self, config: Config) -> None:
        """Set the config object for settings dialog."""
        self._config = config

    def start(self) -> None:
        """Start the tray icon."""
        if not self._enabled:
            return

        # Run GTK in a separate thread
        self._thread = threading.Thread(target=self._run_gtk, daemon=True)
        self._thread.start()
        logger.info("Tray icon started")

    def _run_gtk(self) -> None:
        """Run GTK main loop in background thread."""
        # Create indicator with initial icon
        icon_path = self._get_icon_path(TrayState.IDLE)

        self._indicator = AppIndicator.Indicator.new(
            "autowhisper",
            icon_path,
            AppIndicator.IndicatorCategory.APPLICATION_STATUS
        )
        self._indicator.set_status(AppIndicator.IndicatorStatus.ACTIVE)
        self._indicator.set_title(STATE_TITLES[TrayState.IDLE])

        # Create menu
        menu = Gtk.Menu()

        # Version header
        version_item = Gtk.MenuItem(label=f"AutoWhisper v{VERSION}")
        version_item.set_sensitive(False)
        menu.append(version_item)

        # Separator
        menu.append(Gtk.SeparatorMenuItem())

        # Microphone info
        self._mic_label = Gtk.MenuItem(label=f"Mic: {self._input_device}")
        self._mic_label.set_sensitive(False)
        menu.append(self._mic_label)

        # Speaker info
        self._speaker_label = Gtk.MenuItem(label=f"Speaker: {self._output_device}")
        self._speaker_label.set_sensitive(False)
        menu.append(self._speaker_label)

        # Hotkey info - Recording
        self._trigger_label = Gtk.MenuItem(label=f"Record: {self._format_hotkeys(self._trigger_hotkeys)}")
        self._trigger_label.set_sensitive(False)
        menu.append(self._trigger_label)

        # Hotkey info - Cancel
        self._cancel_label = Gtk.MenuItem(label=f"Cancel: {self._format_hotkeys(self._cancel_hotkeys)}")
        self._cancel_label.set_sensitive(False)
        menu.append(self._cancel_label)

        # Separator
        menu.append(Gtk.SeparatorMenuItem())

        # Settings
        settings_item = Gtk.MenuItem(label="Settings...")
        settings_item.connect("activate", self._on_settings_clicked)
        menu.append(settings_item)

        # Quit
        quit_item = Gtk.MenuItem(label="Quit")
        quit_item.connect("activate", self._on_quit_clicked)
        menu.append(quit_item)

        menu.show_all()
        self._indicator.set_menu(menu)

        # Run GTK main loop
        self._gtk_loop = GLib.MainLoop()
        self._gtk_loop.run()

    def _on_quit_clicked(self, _widget) -> None:
        """Handle quit menu click."""
        logger.info("Quit requested from tray menu")
        if self._on_quit:
            self._on_quit()
        self.stop()

    def _on_settings_clicked(self, _widget) -> None:
        """Handle settings menu click."""
        # Defer to idle to let menu close first
        GLib.idle_add(self._show_settings_dialog)

    def _show_settings_dialog(self) -> bool:
        """Show settings dialog (called from GTK idle)."""
        logger.info("Settings opened - pausing daemon")
        if self._on_settings_open:
            self._on_settings_open()

        if not self._config:
            logger.error("No config available for settings dialog")
            if self._on_settings_close:
                self._on_settings_close()
            return False

        # Show settings dialog with config
        dialog = SettingsDialog(self._config)
        response = dialog.run()

        if response == Gtk.ResponseType.OK:
            changed = False

            # Check hotkey changes
            if dialog.trigger_hotkeys != self._trigger_hotkeys:
                self._trigger_hotkeys = dialog.trigger_hotkeys
                self._update_trigger_label()
                changed = True
            if dialog.cancel_hotkeys != self._cancel_hotkeys:
                self._cancel_hotkeys = dialog.cancel_hotkeys
                self._update_cancel_label()
                changed = True

            # Check device changes
            if dialog.input_device != self._input_device_id:
                self._input_device_id = dialog.input_device
                self._input_device = dialog.input_device_name
                self._update_mic_label(self._input_device)
                changed = True
                logger.info(f"Input device changed to: {self._input_device}")

            if dialog.output_device != self._output_device_id:
                self._output_device_id = dialog.output_device
                self._output_device = dialog.output_device_name
                self._update_speaker_label(self._output_device)
                changed = True
                logger.info(f"Output device changed to: {self._output_device}")

            if changed:
                self._save_settings(
                    dialog.trigger_hotkeys,
                    dialog.cancel_hotkeys,
                    dialog.input_device,
                    dialog.output_device,
                )

        dialog.destroy()

        logger.info("Settings closed - resuming daemon")
        if self._on_settings_close:
            self._on_settings_close()

        return False  # Don't repeat

    def _format_hotkeys(self, hotkeys: list[str]) -> str:
        """Format hotkey list for display."""
        if not hotkeys:
            return "(none)"
        return ", ".join(display_hotkey(hk) for hk in hotkeys)

    def _save_settings(
        self,
        trigger: list[str],
        cancel: list[str],
        input_device: str | None,
        output_device: str | None,
    ) -> None:
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

            # Save hotkeys
            if "hotkeys" not in config:
                config["hotkeys"] = {}
            config["hotkeys"]["trigger"] = trigger
            config["hotkeys"]["cancel"] = cancel

            # Save audio devices
            if "audio" not in config:
                config["audio"] = {}
            if input_device:
                config["audio"]["device"] = input_device
            elif "device" in config["audio"]:
                del config["audio"]["device"]
            if output_device:
                config["audio"]["output_device"] = output_device
            elif "output_device" in config["audio"]:
                del config["audio"]["output_device"]

            with open(config_path, "w") as f:
                toml.dump(config, f)
            logger.info(f"Saved settings to {config_path}")
        except Exception as e:
            logger.error(f"Failed to save settings: {e}")

    def _update_trigger_label(self) -> bool:
        """Update trigger hotkey label (called from GTK thread)."""
        if self._trigger_label:
            self._trigger_label.set_label(f"Record: {self._format_hotkeys(self._trigger_hotkeys)}")
        return False

    def _update_cancel_label(self) -> bool:
        """Update cancel hotkey label (called from GTK thread)."""
        if self._cancel_label:
            self._cancel_label.set_label(f"Cancel: {self._format_hotkeys(self._cancel_hotkeys)}")
        return False

    def set_hotkey(self, hotkey: str | list[str]) -> None:
        """Set the current trigger hotkey(s) display."""
        if isinstance(hotkey, str):
            self._trigger_hotkeys = [hotkey]
        else:
            self._trigger_hotkeys = list(hotkey)
        if self._trigger_label and self._enabled:
            GLib.idle_add(self._update_trigger_label)

    def set_cancel_hotkey(self, hotkey: str | list[str]) -> None:
        """Set the current cancel hotkey(s) display."""
        if isinstance(hotkey, str):
            self._cancel_hotkeys = [hotkey]
        else:
            self._cancel_hotkeys = list(hotkey)
        if self._cancel_label and self._enabled:
            GLib.idle_add(self._update_cancel_label)

    def stop(self) -> None:
        """Stop the tray icon."""
        if self._gtk_loop is not None:
            self._gtk_loop.quit()
            self._gtk_loop = None
            logger.info("Tray icon stopped")

    def set_state(self, state: TrayState) -> None:
        """Update the tray icon state."""
        if not self._enabled or self._indicator is None:
            return

        if state == self._state:
            return

        self._state = state
        icon_path = self._get_icon_path(state)

        # Update icon from GTK thread
        GLib.idle_add(self._update_icon, icon_path, STATE_TITLES[state])
        logger.debug(f"Tray state changed to {state.name}")

    def _update_icon(self, icon_path: str, title: str) -> bool:
        """Update icon (called from GTK thread)."""
        if self._indicator:
            self._indicator.set_icon_full(icon_path, title)
            self._indicator.set_title(title)
        return False  # Don't repeat

    def _get_icon_path(self, state: TrayState) -> str:
        """Get absolute path to icon file for state."""
        name = ICON_NAMES[state]

        # Prefer SVG for best quality
        svg_path = ICONS_DIR / f"{name}.svg"
        if svg_path.exists():
            return str(svg_path.absolute())

        # Fall back to PNG
        png_path = ICONS_DIR / f"{name}.png"
        if png_path.exists():
            return str(png_path.absolute())

        # Use generic icon as last resort
        return "audio-input-microphone"

    def set_input_device(self, device_name: str) -> None:
        """Update the displayed input device name."""
        self._input_device = device_name
        if self._mic_label and self._enabled:
            GLib.idle_add(self._update_mic_label, device_name)

    def _update_mic_label(self, device_name: str) -> bool:
        """Update mic label (called from GTK thread)."""
        if self._mic_label:
            self._mic_label.set_label(f"Mic: {device_name}")
        return False

    def set_output_device(self, device_name: str) -> None:
        """Update the displayed output device name."""
        self._output_device = device_name
        if self._speaker_label and self._enabled:
            GLib.idle_add(self._update_speaker_label, device_name)

    def _update_speaker_label(self, device_name: str) -> bool:
        """Update speaker label (called from GTK thread)."""
        if self._speaker_label:
            self._speaker_label.set_label(f"Speaker: {device_name}")
        return False

    def set_input_device_id(self, device_id: str | None) -> None:
        """Set the input device ID (for config persistence)."""
        self._input_device_id = device_id

    def set_output_device_id(self, device_id: str | None) -> None:
        """Set the output device ID (for config persistence)."""
        self._output_device_id = device_id

    @property
    def enabled(self) -> bool:
        """Check if tray is enabled."""
        return self._enabled

    @property
    def state(self) -> TrayState:
        """Get current state."""
        return self._state

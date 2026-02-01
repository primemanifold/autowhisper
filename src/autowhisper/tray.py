"""System tray icon for AutoWhisper using AppIndicator3."""

from __future__ import annotations

import logging
import threading
from enum import Enum, auto
from pathlib import Path

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


def normalize_key(keyname: str) -> str | None:
    """Normalize key name to match pynput format."""
    key_map = {
        "Shift_L": "shift", "Shift_R": "shift",
        "Control_L": "ctrl", "Control_R": "ctrl",
        "Alt_L": "alt", "Alt_R": "alt",
        "Super_L": "super", "Super_R": "super",
        "Meta_L": "super", "Meta_R": "super",
        "Escape": "esc",
        "Return": "enter",
        "space": "space",
        "Tab": "tab",
    }

    if keyname in key_map:
        return key_map[keyname]

    if len(keyname) == 1:
        return keyname.lower()

    if keyname.startswith("F") and keyname[1:].isdigit():
        return keyname.lower()

    return None


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
            self._shortcut_btn.set_label(self.hotkey)
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
            self._shortcut_btn.set_label("+".join(sorted(self._keys_pressed)) + "...")

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


class HotkeySettingsDialog(Gtk.Dialog):
    """Dialog for configuring hotkeys with grouped sections."""

    def __init__(self, trigger_hotkeys: list[str], cancel_hotkeys: list[str]):
        super().__init__(
            title="Keyboard Shortcuts",
            flags=Gtk.DialogFlags.MODAL,
        )
        self.set_default_size(380, 400)

        # Add buttons
        self.add_button("Cancel", Gtk.ResponseType.CANCEL)
        self.add_button("Save", Gtk.ResponseType.OK)

        # Content in scrolled window for flexibility
        box = self.get_content_area()
        box.set_spacing(12)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_margin_top(12)
        box.set_margin_bottom(8)

        # Recording shortcut group
        self._trigger_group = HotkeyGroup(
            "Recording (hold to speak)",
            trigger_hotkeys,
            "shift+super"
        )
        box.pack_start(self._trigger_group, False, False, 0)

        # Cancel shortcut group
        self._cancel_group = HotkeyGroup(
            "Cancel Recording",
            cancel_hotkeys,
            "esc"
        )
        box.pack_start(self._cancel_group, False, False, 0)

        # Hint
        hint = Gtk.Label()
        hint.set_markup("<small>Click a button then press keys. Backspace clears, Escape cancels.</small>")
        hint.set_opacity(0.6)
        box.pack_start(hint, False, False, 4)

        # Key capture events
        self.connect("key-press-event", self._on_key_press)
        self.connect("key-release-event", self._on_key_release)

        self.show_all()

    @property
    def trigger_hotkeys(self) -> list[str]:
        return self._trigger_group.hotkeys

    @property
    def cancel_hotkeys(self) -> list[str]:
        return self._cancel_group.hotkeys

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
        self._mic_label = None
        self._speaker_label = None
        self._trigger_label = None
        self._cancel_label = None
        self._input_device = "Default"
        self._output_device = "Default"
        self._trigger_hotkeys = ["shift+super"]
        self._cancel_hotkeys = ["esc"]

        if not TRAY_AVAILABLE and enabled:
            logger.warning(
                "Tray icon disabled: AppIndicator3 not available. "
                "Install with: sudo apt install gir1.2-ayatanaappindicator3-0.1"
            )

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

        # Show settings dialog with hotkey lists
        dialog = HotkeySettingsDialog(self._trigger_hotkeys, self._cancel_hotkeys)
        response = dialog.run()

        if response == Gtk.ResponseType.OK:
            changed = False
            if dialog.trigger_hotkeys != self._trigger_hotkeys:
                self._trigger_hotkeys = dialog.trigger_hotkeys
                self._update_trigger_label()
                changed = True
            if dialog.cancel_hotkeys != self._cancel_hotkeys:
                self._cancel_hotkeys = dialog.cancel_hotkeys
                self._update_cancel_label()
                changed = True
            if changed:
                self._save_hotkeys(dialog.trigger_hotkeys, dialog.cancel_hotkeys)
                logger.info(f"Hotkeys changed - trigger: {dialog.trigger_hotkeys}, cancel: {dialog.cancel_hotkeys}")

        dialog.destroy()

        logger.info("Settings closed - resuming daemon")
        if self._on_settings_close:
            self._on_settings_close()

        return False  # Don't repeat

    def _format_hotkeys(self, hotkeys: list[str]) -> str:
        """Format hotkey list for display."""
        if not hotkeys:
            return "(none)"
        return ", ".join(hotkeys)

    def _save_hotkeys(self, trigger: list[str], cancel: list[str]) -> None:
        """Save hotkeys to config file."""
        if not self._config_path:
            return

        try:
            import toml
            config_path = Path(self._config_path)
            if config_path.exists():
                config = toml.load(config_path)
            else:
                config = {}

            if "hotkeys" not in config:
                config["hotkeys"] = {}
            config["hotkeys"]["trigger"] = trigger
            config["hotkeys"]["cancel"] = cancel

            with open(config_path, "w") as f:
                toml.dump(config, f)
            logger.info(f"Saved hotkeys to {config_path}")
        except Exception as e:
            logger.error(f"Failed to save hotkeys: {e}")

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

    @property
    def enabled(self) -> bool:
        """Check if tray is enabled."""
        return self._enabled

    @property
    def state(self) -> TrayState:
        """Get current state."""
        return self._state

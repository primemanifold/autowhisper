"""Global hotkey management using pynput."""

import logging
import queue
import threading
from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional, Set

from pynput import keyboard

from .config import HotkeyConfig

logger = logging.getLogger(__name__)


class HotkeyEvent(Enum):
    """Hotkey events sent to the daemon."""
    START = auto()
    STOP = auto()
    CANCEL = auto()


MODIFIER_NAMES = {"ctrl", "control", "alt", "option", "shift", "super", "win", "cmd", "meta"}


@dataclass
class KeyCombo:
    """Represents a key combination."""
    modifiers: Set[str]
    key: Optional[str]  # None if combo is modifiers-only (like shift+super)
    is_modifier_only: bool = False

    @classmethod
    def parse(cls, combo_str: str) -> "KeyCombo":
        """
        Parse a key combination string like 'ctrl+alt+v' or 'shift+super'.

        Args:
            combo_str: Key combination string

        Returns:
            KeyCombo instance
        """
        parts = combo_str.lower().split("+")
        if len(parts) < 1:
            raise ValueError(f"Invalid key combination: {combo_str}")

        # Normalize all parts
        normalized_parts = []
        for part in parts:
            if part in ("ctrl", "control"):
                normalized_parts.append("ctrl")
            elif part in ("alt", "option"):
                normalized_parts.append("alt")
            elif part in ("shift",):
                normalized_parts.append("shift")
            elif part in ("super", "win", "cmd", "meta"):
                normalized_parts.append("super")
            else:
                normalized_parts.append(part)

        # Check if all parts are modifiers (e.g., shift+super)
        all_modifiers = all(p in ("ctrl", "alt", "shift", "super") for p in normalized_parts)

        if all_modifiers:
            # Modifier-only combo like shift+super
            return cls(modifiers=set(normalized_parts), key=None, is_modifier_only=True)
        else:
            # Regular combo like ctrl+alt+v
            modifiers = set(normalized_parts[:-1]) if len(normalized_parts) > 1 else set()
            key = normalized_parts[-1]
            return cls(modifiers=modifiers, key=key, is_modifier_only=False)


class HotkeyManager:
    """Manages global hotkeys using pynput."""

    def __init__(self, config: HotkeyConfig, event_queue: queue.Queue):
        self.config = config
        self.event_queue = event_queue

        self._trigger_combo = KeyCombo.parse(config.trigger)
        self._cancel_combo = KeyCombo.parse(config.cancel)

        self._pressed_modifiers: Set[str] = set()
        self._trigger_pressed = False
        self._listener: Optional[keyboard.Listener] = None
        self._running = False

    def start(self) -> None:
        """Start listening for hotkeys."""
        if self._running:
            logger.warning("Hotkey listener already running")
            return

        logger.info(
            f"Starting hotkey listener: trigger={self.config.trigger}, "
            f"cancel={self.config.cancel}, mode={self.config.mode}"
        )

        self._listener = keyboard.Listener(
            on_press=self._on_press,
            on_release=self._on_release,
        )
        self._listener.start()
        self._running = True

    def stop(self) -> None:
        """Stop listening for hotkeys."""
        if not self._running:
            return

        logger.info("Stopping hotkey listener")
        self._running = False

        if self._listener:
            self._listener.stop()
            self._listener = None

    def _get_modifier_name(self, key) -> Optional[str]:
        """Convert a pynput key to a modifier name."""
        if hasattr(key, "name"):
            name = key.name.lower()
            if name in ("ctrl", "ctrl_l", "ctrl_r"):
                return "ctrl"
            elif name in ("alt", "alt_l", "alt_r", "alt_gr"):
                return "alt"
            elif name in ("shift", "shift_l", "shift_r"):
                return "shift"
            elif name in ("cmd", "cmd_l", "cmd_r", "super", "super_l", "super_r"):
                return "super"
        return None

    def _get_key_name(self, key) -> str:
        """Get the name of a key."""
        if hasattr(key, "char") and key.char:
            return key.char.lower()
        elif hasattr(key, "name"):
            return key.name.lower()
        return str(key).lower()

    def _check_combo(self, combo: KeyCombo, key_name: str) -> bool:
        """Check if a key combination matches current state."""
        # Check if modifiers match
        if combo.modifiers != self._pressed_modifiers:
            return False

        # Check if key matches
        return key_name == combo.key or key_name == combo.key.replace("_", "")

    def _on_press(self, key) -> None:
        """Handle key press events."""
        if not self._running:
            return

        # Track modifiers
        modifier = self._get_modifier_name(key)
        if modifier:
            self._pressed_modifiers.add(modifier)
            logger.debug(f"Modifier pressed: {modifier}, current: {self._pressed_modifiers}")

            # Check if trigger is modifier-only combo (like shift+super)
            if self._trigger_combo.is_modifier_only:
                if self._trigger_combo.modifiers == self._pressed_modifiers:
                    if not self._trigger_pressed:
                        logger.debug("Modifier-only trigger combo detected!")
                        self._trigger_pressed = True
                        self._send_event(HotkeyEvent.START)
            return

        key_name = self._get_key_name(key)
        logger.debug(f"Key pressed: {key_name}")

        # Check for Escape key to cancel (if enabled)
        if self.config.escape_to_cancel and key_name in ("esc", "escape"):
            logger.debug("Escape pressed, cancelling")
            self._trigger_pressed = False
            self._send_event(HotkeyEvent.CANCEL)
            return

        # Check for cancel hotkey
        if self._check_combo(self._cancel_combo, key_name):
            logger.debug("Cancel hotkey pressed")
            self._trigger_pressed = False
            self._send_event(HotkeyEvent.CANCEL)
            return

        # Check for trigger hotkey (non-modifier-only)
        if not self._trigger_combo.is_modifier_only and self._check_combo(self._trigger_combo, key_name):
            if self.config.mode == "toggle":
                # Toggle mode: alternate between start/stop
                if self._trigger_pressed:
                    self._trigger_pressed = False
                    self._send_event(HotkeyEvent.STOP)
                else:
                    self._trigger_pressed = True
                    self._send_event(HotkeyEvent.START)
            else:
                # Push-to-talk mode: start on press
                if not self._trigger_pressed:
                    self._trigger_pressed = True
                    self._send_event(HotkeyEvent.START)

    def _on_release(self, key) -> None:
        """Handle key release events."""
        if not self._running:
            return

        # Track modifiers
        modifier = self._get_modifier_name(key)
        if modifier:
            self._pressed_modifiers.discard(modifier)
            logger.debug(f"Modifier released: {modifier}, current: {self._pressed_modifiers}")

            # For push-to-talk with modifier-only combos
            if (
                self._trigger_combo.is_modifier_only
                and self.config.mode == "push_to_talk"
                and self._trigger_pressed
                and modifier in self._trigger_combo.modifiers
            ):
                logger.debug("Modifier-only trigger released, stopping")
                self._trigger_pressed = False
                self._send_event(HotkeyEvent.STOP)
            return

        key_name = self._get_key_name(key)

        # Push-to-talk: stop on release (for non-modifier-only combos)
        if (
            not self._trigger_combo.is_modifier_only
            and self.config.mode == "push_to_talk"
            and self._trigger_pressed
        ):
            if self._check_combo(self._trigger_combo, key_name):
                self._trigger_pressed = False
                self._send_event(HotkeyEvent.STOP)

    def _send_event(self, event: HotkeyEvent) -> None:
        """Send an event to the daemon."""
        logger.debug(f"Hotkey event: {event.name}")
        try:
            self.event_queue.put_nowait(event)
        except queue.Full:
            logger.warning("Event queue full, dropping event")

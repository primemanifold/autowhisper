"""Text output management (X11 injection, xdotool, clipboard)."""

import logging
import subprocess
import time

from .config import OutputConfig

logger = logging.getLogger(__name__)


class OutputManager:
    """Manages text output to the active window."""

    def __init__(self, config: OutputConfig):
        self.config = config
        self._xdotool_available: bool | None = None
        self._xclip_available: bool | None = None
        self._xlib_available: bool | None = None

    def initialize(self) -> None:
        """Check available output methods."""
        # Check for xdotool
        try:
            result = subprocess.run(
                ["which", "xdotool"],
                capture_output=True,
                timeout=5,
            )
            self._xdotool_available = result.returncode == 0
        except Exception:
            self._xdotool_available = False

        # Check for xclip
        try:
            result = subprocess.run(
                ["which", "xclip"],
                capture_output=True,
                timeout=5,
            )
            self._xclip_available = result.returncode == 0
        except Exception:
            self._xclip_available = False

        # Check for python-xlib
        try:
            from Xlib import XK, X, display
            self._xlib_available = True
        except ImportError:
            self._xlib_available = False

        logger.info(
            f"Output methods: xlib={self._xlib_available}, "
            f"xdotool={self._xdotool_available}, xclip={self._xclip_available}"
        )

    def inject(self, text: str) -> bool:
        """
        Inject text into the active window.

        Uses graceful degradation: X11 → xdotool → clipboard + paste

        Args:
            text: Text to inject

        Returns:
            True if successful, False otherwise
        """
        if not text:
            logger.debug("Empty text, skipping injection")
            return True

        # Apply text transformations
        if self.config.lowercase:
            text = text.lower()

        # Handle ending_action: "newline" appends \n to text
        if self.config.ending_action == "newline":
            text += "\n"

        # Try injection methods in order of preference
        if self.config.method == "inject":
            # Also copy to clipboard if enabled (for later Ctrl+V paste)
            if self.config.also_copy_to_clipboard:
                self._copy_to_clipboard(text)

            # Try X11 direct injection first
            if self._xlib_available and self._inject_xlib(text):
                # Handle ending_action: "return_key" sends Return keypress after text
                if self.config.ending_action == "return_key":
                    self._send_return_key()
                return True

            # Fall back to xdotool
            if self._xdotool_available and self._inject_xdotool(text):
                # Handle ending_action: "return_key" sends Return keypress after text
                if self.config.ending_action == "return_key":
                    self._send_return_key()
                return True

            # Fall back to clipboard + paste
            logger.warning("Direct injection failed, falling back to clipboard")

        # Clipboard method (or fallback)
        result = self._inject_clipboard(text)
        # Handle ending_action: "return_key" sends Return keypress after paste
        if result and self.config.ending_action == "return_key":
            self._send_return_key()
        return result

    def _inject_xlib(self, text: str) -> bool:
        """Inject text using python-xlib (direct X11)."""
        try:
            from Xlib import XK, X, display
            from Xlib.protocol import event

            d = display.Display()
            root = d.screen().root

            # Get the focused window
            focus = d.get_input_focus().focus

            if focus == X.NONE or focus == X.PointerRoot:
                logger.warning("No focused window for X11 injection")
                return False

            # Send key events for each character
            for char in text:
                # Handle special characters
                if char == '\n':
                    keysym = XK.XK_Return
                elif char == '\t':
                    keysym = XK.XK_Tab
                else:
                    keysym = XK.string_to_keysym(char)
                    if keysym == 0:
                        keysym = ord(char)

                keycode = d.keysym_to_keycode(keysym)
                if keycode == 0:
                    logger.debug(f"No keycode for character: {char}")
                    continue

                # Check if shift is needed
                shift_needed = char.isupper() or char in '~!@#$%^&*()_+{}|:"<>?'

                # Press shift if needed
                if shift_needed:
                    shift_keycode = d.keysym_to_keycode(XK.XK_Shift_L)
                    key_event = event.KeyPress(
                        time=X.CurrentTime,
                        root=root,
                        window=focus,
                        same_screen=True,
                        child=X.NONE,
                        root_x=0,
                        root_y=0,
                        event_x=0,
                        event_y=0,
                        state=0,
                        detail=shift_keycode,
                    )
                    focus.send_event(key_event, propagate=True)

                # Key press
                state = X.ShiftMask if shift_needed else 0
                key_event = event.KeyPress(
                    time=X.CurrentTime,
                    root=root,
                    window=focus,
                    same_screen=True,
                    child=X.NONE,
                    root_x=0,
                    root_y=0,
                    event_x=0,
                    event_y=0,
                    state=state,
                    detail=keycode,
                )
                focus.send_event(key_event, propagate=True)

                # Key release
                key_event = event.KeyRelease(
                    time=X.CurrentTime,
                    root=root,
                    window=focus,
                    same_screen=True,
                    child=X.NONE,
                    root_x=0,
                    root_y=0,
                    event_x=0,
                    event_y=0,
                    state=state,
                    detail=keycode,
                )
                focus.send_event(key_event, propagate=True)

                # Release shift if needed
                if shift_needed:
                    key_event = event.KeyRelease(
                        time=X.CurrentTime,
                        root=root,
                        window=focus,
                        same_screen=True,
                        child=X.NONE,
                        root_x=0,
                        root_y=0,
                        event_x=0,
                        event_y=0,
                        state=0,
                        detail=shift_keycode,
                    )
                    focus.send_event(key_event, propagate=True)

            d.sync()
            d.close()

            logger.debug(f"Injected {len(text)} characters via X11")
            return True

        except Exception as e:
            logger.warning(f"X11 injection failed: {e}")
            return False

    def _inject_xdotool(self, text: str) -> bool:
        """Inject text using xdotool."""
        try:
            # Use xdotool type with --clearmodifiers to handle stuck modifiers
            result = subprocess.run(
                ["xdotool", "type", "--clearmodifiers", "--", text],
                timeout=10,
                capture_output=True,
            )

            if result.returncode == 0:
                logger.debug(f"Injected {len(text)} characters via xdotool")
                return True
            else:
                logger.warning(f"xdotool failed: {result.stderr.decode()}")
                return False

        except subprocess.TimeoutExpired:
            logger.warning("xdotool timed out")
            return False
        except Exception as e:
            logger.warning(f"xdotool injection failed: {e}")
            return False

    def _copy_to_clipboard(self, text: str) -> bool:
        """Copy text to clipboard (without pasting)."""
        try:
            # Copy to clipboard using xclip
            if self._xclip_available:
                proc = subprocess.Popen(
                    ["xclip", "-selection", "clipboard"],
                    stdin=subprocess.PIPE,
                )
                proc.communicate(input=text.encode("utf-8"), timeout=5)

                if proc.returncode != 0:
                    logger.warning("xclip failed to copy to clipboard")
                    return False
            else:
                # Try using xsel as fallback
                proc = subprocess.Popen(
                    ["xsel", "--clipboard", "--input"],
                    stdin=subprocess.PIPE,
                )
                proc.communicate(input=text.encode("utf-8"), timeout=5)

                if proc.returncode != 0:
                    logger.warning("xsel failed to copy to clipboard")
                    return False

            logger.debug(f"Copied {len(text)} characters to clipboard")
            return True

        except Exception as e:
            logger.warning(f"Clipboard copy failed: {e}")
            return False

    def _inject_clipboard(self, text: str) -> bool:
        """Copy text to clipboard and optionally paste."""
        if not self._copy_to_clipboard(text):
            return False

        if self.config.auto_paste:
            time.sleep(self.config.paste_delay)
            return self._send_paste()

        return True

    def _send_paste(self) -> bool:
        """Send Ctrl+V paste command."""
        try:
            if self._xdotool_available:
                result = subprocess.run(
                    ["xdotool", "key", "--clearmodifiers", "ctrl+v"],
                    timeout=5,
                    capture_output=True,
                )
                return result.returncode == 0
            else:
                logger.warning("Cannot send paste: xdotool not available")
                return False
        except Exception as e:
            logger.warning(f"Paste failed: {e}")
            return False

    def _send_return_key(self) -> bool:
        """Send Return/Enter keypress."""
        try:
            if self._xdotool_available:
                # Small delay to let the application process the injected text
                time.sleep(0.05)
                result = subprocess.run(
                    ["xdotool", "key", "--clearmodifiers", "Return"],
                    timeout=5,
                    capture_output=True,
                )
                if result.returncode == 0:
                    logger.debug("Sent Return keypress via xdotool")
                    return True
                else:
                    logger.warning(f"xdotool key Return failed: {result.stderr.decode()}")
                    return False
            else:
                logger.warning("Cannot send Return key: xdotool not available")
                return False
        except Exception as e:
            logger.warning(f"Return keypress failed: {e}")
            return False

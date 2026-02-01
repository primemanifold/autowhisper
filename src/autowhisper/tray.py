"""System tray icon for AutoWhisper."""

from __future__ import annotations

import logging
import threading
from enum import Enum, auto
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import pystray
    from PIL import Image

try:
    import pystray
    from PIL import Image, ImageDraw
    TRAY_AVAILABLE = True
except ImportError:
    TRAY_AVAILABLE = False

logger = logging.getLogger(__name__)


class TrayState(Enum):
    """Tray icon states."""
    IDLE = auto()
    RECORDING = auto()
    PROCESSING = auto()
    ERROR = auto()


# State colors (RGB)
STATE_COLORS = {
    TrayState.IDLE: (74, 222, 128),        # Green
    TrayState.RECORDING: (248, 113, 113),  # Red
    TrayState.PROCESSING: (251, 191, 36),  # Amber
    TrayState.ERROR: (239, 68, 68),        # Bright Red
}


class TrayManager:
    """Manages the system tray icon."""

    def __init__(self, enabled: bool = True):
        self._enabled = enabled and TRAY_AVAILABLE
        self._icon: pystray.Icon | None = None
        self._state = TrayState.IDLE
        self._thread: threading.Thread | None = None
        self._icons: dict[TrayState, Image.Image] = {}

        if not TRAY_AVAILABLE and enabled:
            logger.warning(
                "Tray icon disabled: pystray or Pillow not installed. "
                "Install with: pip install pystray Pillow"
            )

    def start(self) -> None:
        """Start the tray icon."""
        if not self._enabled:
            return

        # Pre-generate icons for all states
        self._generate_icons()

        # Create the icon
        self._icon = pystray.Icon(
            name="autowhisper",
            icon=self._icons[TrayState.IDLE],
            title="AutoWhisper - Ready",
        )

        # Run in a separate thread
        self._thread = threading.Thread(target=self._icon.run, daemon=True)
        self._thread.start()
        logger.info("Tray icon started")

    def stop(self) -> None:
        """Stop the tray icon."""
        if self._icon is not None:
            self._icon.stop()
            self._icon = None
            logger.info("Tray icon stopped")

    def set_state(self, state: TrayState) -> None:
        """Update the tray icon state."""
        if not self._enabled or self._icon is None:
            return

        if state == self._state:
            return

        self._state = state
        self._icon.icon = self._icons[state]
        self._icon.title = self._get_title(state)
        logger.debug(f"Tray state changed to {state.name}")

    def _get_title(self, state: TrayState) -> str:
        """Get tooltip title for state."""
        titles = {
            TrayState.IDLE: "AutoWhisper - Ready",
            TrayState.RECORDING: "AutoWhisper - Recording...",
            TrayState.PROCESSING: "AutoWhisper - Transcribing...",
            TrayState.ERROR: "AutoWhisper - Error",
        }
        return titles.get(state, "AutoWhisper")

    def _generate_icons(self) -> None:
        """Generate icons for all states."""
        for state in TrayState:
            self._icons[state] = self._create_icon(STATE_COLORS[state])

    def _create_icon(self, color: tuple[int, int, int], size: int = 64) -> Image.Image:
        """Create a circle wave icon with bars."""
        img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)

        cx, cy = size // 2, size // 2
        r = 26

        # Draw circle outline
        draw.ellipse([cx-r, cy-r, cx+r, cy+r], outline=color, width=2)

        # Draw waveform bars inside (with padding from circle edge)
        bar_width = 3
        bar_gap = 3
        heights = [10, 18, 24, 18, 10]
        total = len(heights) * bar_width + (len(heights) - 1) * bar_gap
        x = cx - total // 2
        for h in heights:
            y1 = cy - h // 2
            y2 = cy + h // 2
            draw.rounded_rectangle([x, y1, x + bar_width, y2], radius=2, fill=color)
            x += bar_width + bar_gap

        return img

    @property
    def enabled(self) -> bool:
        """Check if tray is enabled."""
        return self._enabled

    @property
    def state(self) -> TrayState:
        """Get current state."""
        return self._state

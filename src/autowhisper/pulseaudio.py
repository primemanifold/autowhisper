"""PulseAudio manager for muting other applications during recording."""

import logging
import os
import threading
from dataclasses import dataclass

logger = logging.getLogger(__name__)

# Try to import pulsectl, but gracefully handle if not available
try:
    import pulsectl
    PULSECTL_AVAILABLE = True
except ImportError:
    pulsectl = None
    PULSECTL_AVAILABLE = False


@dataclass
class MutedSinkInput:
    """Represents a muted sink input with its original state."""
    index: int
    name: str
    was_muted: bool  # Original state to restore


class PulseAudioManager:
    """Manages muting/unmuting of other applications via PulseAudio."""

    EXCLUDED_APPS = {"python", "python3", "autowhisper", "sounddevice"}

    def __init__(self, enabled: bool, beep_duration: float):
        """
        Initialize the PulseAudio manager.

        Args:
            enabled: Whether app muting feature is enabled
            beep_duration: Duration of the start beep in seconds
        """
        self._enabled = enabled
        self._beep_duration = beep_duration
        self._pulse: pulsectl.Pulse | None = None
        self._muted_inputs: list[MutedSinkInput] = []
        self._lock = threading.Lock()
        self._mute_timer: threading.Timer | None = None
        self._initialized = False

    def initialize(self) -> bool:
        """
        Initialize the PulseAudio connection.

        Returns:
            True if initialization succeeded or feature is disabled/unavailable
            (graceful degradation), False only on catastrophic errors.
        """
        if not self._enabled:
            logger.debug("PulseAudio muting disabled by configuration")
            return True

        if not PULSECTL_AVAILABLE:
            logger.warning(
                "pulsectl not available, app muting feature disabled. "
                "Install with: pip install pulsectl"
            )
            self._enabled = False
            return True

        try:
            self._pulse = pulsectl.Pulse("autowhisper")
            self._initialized = True
            logger.info("PulseAudio muting enabled")
            return True
        except Exception as e:
            logger.warning(f"Failed to connect to PulseAudio: {e}. App muting disabled.")
            self._enabled = False
            return True

    def _is_excluded(self, sink_input) -> bool:
        """
        Check if a sink input should be excluded from muting.

        Args:
            sink_input: PulseAudio sink input object

        Returns:
            True if the sink input should NOT be muted
        """
        proplist = sink_input.proplist
        binary = proplist.get("application.process.binary", "")

        # Get the basename (filename without path)
        basename = os.path.basename(binary)

        if basename in self.EXCLUDED_APPS:
            logger.debug(f"Excluding sink input '{sink_input.name}' (binary: {basename})")
            return True

        return False

    def _do_mute(self) -> None:
        """Actually perform the muting of other apps (called after delay)."""
        if not self._enabled or not self._initialized or not self._pulse:
            return

        with self._lock:
            try:
                sink_inputs = self._pulse.sink_input_list()
                muted_count = 0

                for sink_input in sink_inputs:
                    if self._is_excluded(sink_input):
                        continue

                    # Track original mute state
                    was_muted = sink_input.mute == 1
                    self._muted_inputs.append(
                        MutedSinkInput(
                            index=sink_input.index,
                            name=sink_input.name or "Unknown",
                            was_muted=was_muted,
                        )
                    )

                    # Only mute if not already muted
                    if not was_muted:
                        self._pulse.sink_input_mute(sink_input.index, mute=True)
                        muted_count += 1
                        logger.debug(f"Muted: {sink_input.name}")

                if muted_count > 0:
                    logger.debug(f"Muted {muted_count} other app(s)")

            except Exception as e:
                logger.warning(f"Error muting other apps: {e}")
                # Clear tracked inputs on error to avoid issues during unmute
                self._muted_inputs.clear()

    def mute_other_apps(self, delay: bool = True) -> None:
        """
        Mute all other applications' audio.

        Args:
            delay: If True, wait for beep_duration + 0.05s before muting
                   so user hears the start beep
        """
        if not self._enabled or not self._initialized:
            return

        # Cancel any pending mute timer
        if self._mute_timer is not None:
            self._mute_timer.cancel()
            self._mute_timer = None

        if delay:
            # Schedule muting after beep finishes
            delay_seconds = self._beep_duration + 0.05
            logger.debug(f"Scheduling app mute in {delay_seconds:.2f}s")
            self._mute_timer = threading.Timer(delay_seconds, self._do_mute)
            self._mute_timer.daemon = True
            self._mute_timer.start()
        else:
            self._do_mute()

    def unmute_other_apps(self) -> None:
        """Restore original mute state for all previously muted apps."""
        if not self._enabled or not self._initialized or not self._pulse:
            return

        # Cancel any pending mute timer (in case unmute called before mute executed)
        if self._mute_timer is not None:
            self._mute_timer.cancel()
            self._mute_timer = None

        with self._lock:
            if not self._muted_inputs:
                return

            restored_count = 0

            for muted_input in self._muted_inputs:
                # Only unmute apps that weren't muted before we muted them
                if not muted_input.was_muted:
                    try:
                        self._pulse.sink_input_mute(muted_input.index, mute=False)
                        restored_count += 1
                        logger.debug(f"Unmuted: {muted_input.name}")
                    except Exception as e:
                        # Sink input may no longer exist (app closed)
                        logger.debug(
                            f"Could not unmute {muted_input.name} "
                            f"(index {muted_input.index}): {e}"
                        )

            if restored_count > 0:
                logger.debug(f"Restored audio for {restored_count} app(s)")

            self._muted_inputs.clear()

    def cleanup(self) -> None:
        """Clean up resources on shutdown."""
        # Cancel any pending mute timer
        if self._mute_timer is not None:
            self._mute_timer.cancel()
            self._mute_timer = None

        # Restore any muted apps
        self.unmute_other_apps()

        # Close PulseAudio connection
        if self._pulse is not None:
            try:
                self._pulse.close()
            except Exception as e:
                logger.debug(f"Error closing PulseAudio connection: {e}")
            self._pulse = None

        self._initialized = False
        logger.debug("PulseAudio manager cleaned up")

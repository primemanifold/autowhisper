"""Main daemon with state machine and event loop."""

import logging
import os
import queue
import signal
import sys
import threading
import time
from enum import Enum, auto
from pathlib import Path
from typing import Optional

from .audio import AudioManager
from .config import Config
from .feedback import FeedbackManager
from .hotkey import HotkeyEvent, HotkeyManager
from .inference import WhisperInference
from .output import OutputManager
from .tray import TrayManager, TrayState

logger = logging.getLogger(__name__)


class DaemonState(Enum):
    """Daemon state machine states."""
    IDLE = auto()
    RECORDING = auto()
    PROCESSING = auto()
    SHUTDOWN = auto()


class AutoWhisperDaemon:
    """Main daemon that orchestrates all components."""

    def __init__(self, config: Config):
        self.config = config
        self._state = DaemonState.IDLE
        self._event_queue: queue.Queue[HotkeyEvent] = queue.Queue(maxsize=100)
        self._shutdown_event = threading.Event()

        # Initialize components
        self._audio = AudioManager(config.audio)
        self._whisper = WhisperInference(config.model)
        self._output = OutputManager(config.output)
        self._feedback = FeedbackManager(config.feedback)
        self._hotkey = HotkeyManager(config.hotkeys, self._event_queue)
        self._tray = TrayManager(config.tray.enabled)

        # Minimum recording duration (seconds)
        self._min_duration = 0.5

    def initialize(self) -> None:
        """Initialize all components."""
        logger.info("Initializing AutoWhisper daemon")

        # Set up signal handlers
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

        # Write PID file
        self._write_pid_file()

        # Initialize components
        logger.info("Initializing audio subsystem")
        self._audio.initialize()

        logger.info("Loading Whisper model")
        self._whisper.load()

        logger.info("Initializing output subsystem")
        self._output.initialize()

        logger.info("Starting tray icon")
        self._tray.start()

        logger.info("Initialization complete")

    def run(self) -> None:
        """Run the main event loop."""
        logger.info("Starting AutoWhisper daemon")

        # Start hotkey listener
        self._hotkey.start()

        try:
            while not self._shutdown_event.is_set():
                self._process_events()
        except Exception as e:
            logger.exception(f"Fatal error in event loop: {e}")
            self._feedback.play_error()
        finally:
            self._cleanup()

    def _process_events(self) -> None:
        """Process events from the queue."""
        try:
            # Wait for event with timeout for shutdown check
            event = self._event_queue.get(timeout=0.1)
        except queue.Empty:
            return

        logger.debug(f"Processing event: {event.name} in state {self._state.name}")

        if event == HotkeyEvent.START:
            self._handle_start()
        elif event == HotkeyEvent.STOP:
            self._handle_stop()
        elif event == HotkeyEvent.CANCEL:
            self._handle_cancel()

    def _handle_start(self) -> None:
        """Handle START event."""
        if self._state != DaemonState.IDLE:
            logger.debug(f"Ignoring START in state {self._state.name}")
            return

        logger.info("Starting recording")
        self._state = DaemonState.RECORDING
        self._tray.set_state(TrayState.RECORDING)
        self._feedback.play_start()
        self._audio.start_recording()

    def _handle_stop(self) -> None:
        """Handle STOP event."""
        if self._state != DaemonState.RECORDING:
            logger.debug(f"Ignoring STOP in state {self._state.name}")
            return

        logger.info("Stopping recording")
        self._state = DaemonState.PROCESSING
        self._tray.set_state(TrayState.PROCESSING)
        self._feedback.play_stop()

        # Get recorded audio
        audio = self._audio.stop_recording()
        duration = len(audio) / self.config.audio.sample_rate

        if duration < self._min_duration:
            logger.warning(f"Recording too short ({duration:.2f}s), skipping")
            self._feedback.play_short_beep()
            self._state = DaemonState.IDLE
            self._tray.set_state(TrayState.IDLE)
            return

        # Trim silence
        audio = self._audio.trim_silence(audio)
        if len(audio) == 0:
            logger.warning("No audio after silence trimming")
            self._state = DaemonState.IDLE
            self._tray.set_state(TrayState.IDLE)
            return

        # Transcribe
        try:
            logger.info(f"Transcribing {duration:.2f}s of audio")
            text = self._whisper.transcribe(audio)

            if text:
                logger.info(f"Transcription: {text[:50]}{'...' if len(text) > 50 else ''}")
                success = self._output.inject(text)
                if not success:
                    logger.warning("Text injection failed")
                    self._feedback.play_error()
            else:
                logger.info("Empty transcription result")

        except Exception as e:
            logger.exception(f"Transcription error: {e}")
            self._feedback.play_error()

        self._state = DaemonState.IDLE
        self._tray.set_state(TrayState.IDLE)

    def _handle_cancel(self) -> None:
        """Handle CANCEL event."""
        if self._state == DaemonState.RECORDING:
            logger.info("Canceling recording")
            self._audio.stop_recording()
            self._feedback.play_error()
        elif self._state == DaemonState.PROCESSING:
            logger.info("Cannot cancel during processing")
            return

        self._state = DaemonState.IDLE
        self._tray.set_state(TrayState.IDLE)

    def _signal_handler(self, signum: int, frame) -> None:
        """Handle shutdown signals."""
        sig_name = signal.Signals(signum).name
        logger.info(f"Received {sig_name}, shutting down")
        self._state = DaemonState.SHUTDOWN
        self._shutdown_event.set()

    def _write_pid_file(self) -> None:
        """Write PID file."""
        pid_file = Path(self.config.daemon.pid_file)
        try:
            pid_file.parent.mkdir(parents=True, exist_ok=True)
            pid_file.write_text(str(os.getpid()))
            logger.debug(f"Wrote PID {os.getpid()} to {pid_file}")
        except Exception as e:
            logger.warning(f"Failed to write PID file: {e}")

    def _remove_pid_file(self) -> None:
        """Remove PID file."""
        pid_file = Path(self.config.daemon.pid_file)
        try:
            if pid_file.exists():
                pid_file.unlink()
                logger.debug(f"Removed PID file {pid_file}")
        except Exception as e:
            logger.warning(f"Failed to remove PID file: {e}")

    def _cleanup(self) -> None:
        """Clean up resources on shutdown."""
        logger.info("Cleaning up")

        # Stop hotkey listener
        self._hotkey.stop()

        # Stop tray icon
        self._tray.stop()

        # Stop recording if active
        if self._audio.is_recording():
            self._audio.stop_recording()

        # Remove PID file
        self._remove_pid_file()

        logger.info("Cleanup complete")

    @property
    def state(self) -> DaemonState:
        """Get current daemon state."""
        return self._state

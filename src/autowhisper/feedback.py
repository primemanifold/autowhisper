"""Audio feedback (beeps) for user interaction."""

import logging
import threading
from typing import Optional

import numpy as np

from .config import FeedbackConfig

logger = logging.getLogger(__name__)


class FeedbackManager:
    """Manages audio feedback sounds."""

    def __init__(self, config: FeedbackConfig):
        self.config = config
        self._sample_rate = 44100
        self._lock = threading.Lock()

    def _generate_tone(
        self,
        frequency: int,
        duration: float,
        volume: float,
    ) -> np.ndarray:
        """
        Generate a sine wave tone.

        Args:
            frequency: Tone frequency in Hz
            duration: Duration in seconds
            volume: Volume (0.0 to 1.0)

        Returns:
            Audio samples as float32 numpy array
        """
        t = np.linspace(
            0,
            duration,
            int(self._sample_rate * duration),
            dtype=np.float32,
        )

        # Generate sine wave with fade in/out
        wave = np.sin(2 * np.pi * frequency * t)

        # Apply envelope (fade in/out to avoid clicks)
        fade_samples = int(0.01 * self._sample_rate)  # 10ms fade
        if len(wave) > fade_samples * 2:
            fade_in = np.linspace(0, 1, fade_samples)
            fade_out = np.linspace(1, 0, fade_samples)
            wave[:fade_samples] *= fade_in
            wave[-fade_samples:] *= fade_out

        # Apply volume (keep as float32 for sounddevice)
        wave = (wave * volume).astype(np.float32)

        return wave

    def _play_audio(self, audio: np.ndarray) -> None:
        """Play audio samples using sounddevice (more compatible than simpleaudio)."""
        try:
            import sounddevice as sd

            with self._lock:
                # Play audio non-blocking
                sd.play(audio, samplerate=self._sample_rate)
        except Exception as e:
            logger.warning(f"Failed to play audio feedback: {e}")

    def play_start(self) -> None:
        """Play the start recording beep."""
        if not self.config.enabled:
            return

        logger.debug(f"Playing start beep ({self.config.frequency_start}Hz)")
        audio = self._generate_tone(
            self.config.frequency_start,
            self.config.duration,
            self.config.volume,
        )
        threading.Thread(
            target=self._play_audio,
            args=(audio,),
            daemon=True,
        ).start()

    def play_stop(self) -> None:
        """Play the stop recording beep."""
        if not self.config.enabled:
            return

        logger.debug(f"Playing stop beep ({self.config.frequency_stop}Hz)")
        audio = self._generate_tone(
            self.config.frequency_stop,
            self.config.duration,
            self.config.volume,
        )
        threading.Thread(
            target=self._play_audio,
            args=(audio,),
            daemon=True,
        ).start()

    def play_error(self) -> None:
        """Play the error beep."""
        if not self.config.enabled:
            return

        logger.debug(f"Playing error beep ({self.config.frequency_error}Hz)")
        # Error beep is slightly longer
        audio = self._generate_tone(
            self.config.frequency_error,
            self.config.duration * 2,
            self.config.volume,
        )
        threading.Thread(
            target=self._play_audio,
            args=(audio,),
            daemon=True,
        ).start()

    def play_short_beep(self) -> None:
        """Play a short low beep (for too-short recordings)."""
        if not self.config.enabled:
            return

        logger.debug("Playing short beep (recording too short)")
        audio = self._generate_tone(
            300,  # Low frequency
            self.config.duration / 2,
            self.config.volume * 0.5,
        )
        threading.Thread(
            target=self._play_audio,
            args=(audio,),
            daemon=True,
        ).start()

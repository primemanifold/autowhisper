"""Audio capture and VAD processing."""

import logging
import threading
from collections import deque
from typing import Callable, Optional

import numpy as np
import sounddevice as sd

from .config import AudioConfig

logger = logging.getLogger(__name__)


class SileroVAD:
    """Silero VAD wrapper for voice activity detection."""

    def __init__(self, threshold: float = 0.5):
        self.threshold = threshold
        self.model = None
        self._sample_rate = 16000

    def load(self) -> None:
        """Load the Silero VAD model."""
        import torch

        logger.info("Loading Silero VAD model")
        self.model, utils = torch.hub.load(
            repo_or_dir="snakers4/silero-vad",
            model="silero_vad",
            force_reload=False,
            onnx=False,
        )
        self.model.eval()
        logger.info("Silero VAD loaded")

    def is_speech(self, audio: np.ndarray) -> bool:
        """
        Check if audio chunk contains speech.

        Args:
            audio: Audio samples as float32 numpy array

        Returns:
            True if speech detected, False otherwise
        """
        if self.model is None:
            return True  # Assume speech if VAD not loaded

        import torch

        # Silero VAD expects 512 samples at 16kHz
        if len(audio) < 512:
            return False

        # Use last 512 samples
        chunk = audio[-512:]
        tensor = torch.from_numpy(chunk).float()

        with torch.no_grad():
            prob = self.model(tensor, self._sample_rate).item()

        return prob >= self.threshold

    def reset(self) -> None:
        """Reset VAD state."""
        if self.model is not None:
            self.model.reset_states()


class AudioManager:
    """Manages audio capture with sounddevice."""

    def __init__(self, config: AudioConfig):
        self.config = config
        self._buffer: list[np.ndarray] = []
        self._recording = False
        self._stream: Optional[sd.InputStream] = None
        self._lock = threading.Lock()
        self._vad: Optional[SileroVAD] = None
        self._speech_detected = False
        self._silence_samples = 0
        self._max_samples = int(config.max_duration * config.sample_rate)
        self._silence_threshold_samples = int(
            config.silence_duration * config.sample_rate
        )
        self._input_device_name = "Unknown"
        self._output_device_name = "Unknown"

    def initialize(self) -> None:
        """Initialize audio system and optionally load VAD."""
        logger.info(
            f"Initializing audio: {self.config.sample_rate}Hz, "
            f"buffer_size={self.config.buffer_size}"
        )

        if self.config.vad_enabled:
            self._vad = SileroVAD(threshold=self.config.vad_threshold)
            self._vad.load()

        # Query audio devices
        try:
            default_input = sd.query_devices(kind="input")
            self._input_device_name = default_input['name']
            logger.info(f"Default input device: {self._input_device_name}")
        except Exception as e:
            logger.warning(f"Could not query input device: {e}")

        try:
            default_output = sd.query_devices(kind="output")
            self._output_device_name = default_output['name']
            logger.info(f"Default output device: {self._output_device_name}")
        except Exception as e:
            logger.warning(f"Could not query output device: {e}")

    @property
    def input_device_name(self) -> str:
        """Get the name of the input device."""
        return self._input_device_name

    @property
    def output_device_name(self) -> str:
        """Get the name of the output device."""
        return self._output_device_name

    def start_recording(self) -> None:
        """Start capturing audio."""
        with self._lock:
            if self._recording:
                logger.warning("Already recording")
                return

            self._buffer = []
            self._speech_detected = False
            self._silence_samples = 0

            if self._vad:
                self._vad.reset()

            device = self.config.device
            if device == "default":
                device = None

            self._stream = sd.InputStream(
                samplerate=self.config.sample_rate,
                channels=self.config.channels,
                dtype=np.float32,
                blocksize=self.config.buffer_size,
                device=device,
                callback=self._audio_callback,
            )
            self._stream.start()
            self._recording = True
            logger.debug("Recording started")

    def stop_recording(self) -> np.ndarray:
        """
        Stop recording and return captured audio.

        Returns:
            Audio samples as float32 numpy array
        """
        with self._lock:
            if not self._recording:
                logger.warning("Not recording")
                return np.array([], dtype=np.float32)

            self._recording = False

            if self._stream:
                self._stream.stop()
                self._stream.close()
                self._stream = None

            if not self._buffer:
                return np.array([], dtype=np.float32)

            audio = np.concatenate(self._buffer)
            self._buffer = []

            logger.debug(
                f"Recording stopped: {len(audio)} samples "
                f"({len(audio) / self.config.sample_rate:.2f}s)"
            )

            return audio

    def _audio_callback(
        self,
        indata: np.ndarray,
        frames: int,
        time_info: dict,
        status: sd.CallbackFlags,
    ) -> None:
        """Sounddevice callback for audio capture."""
        if status:
            logger.warning(f"Audio callback status: {status}")

        if not self._recording:
            return

        # Convert to mono and copy (sounddevice reuses buffers)
        # Avoid double-copy: flatten() already copies, [:, 0] needs explicit copy
        if indata.ndim > 1:
            audio = indata[:, 0].copy()
        else:
            audio = indata.flatten()  # Already a copy

        # Check total duration
        total_samples = sum(len(chunk) for chunk in self._buffer) + len(audio)
        if total_samples >= self._max_samples:
            logger.warning("Max recording duration reached")
            return

        # VAD processing
        if self._vad and self.config.vad_enabled:
            if self._vad.is_speech(audio):
                self._speech_detected = True
                self._silence_samples = 0
            else:
                self._silence_samples += len(audio)

        self._buffer.append(audio)

    def is_recording(self) -> bool:
        """Check if currently recording."""
        return self._recording

    def get_duration(self) -> float:
        """Get current recording duration in seconds."""
        with self._lock:
            total_samples = sum(len(chunk) for chunk in self._buffer)
            return total_samples / self.config.sample_rate

    def trim_silence(self, audio: np.ndarray, threshold: float = 0.01) -> np.ndarray:
        """
        Trim leading and trailing silence from audio.

        Args:
            audio: Audio samples
            threshold: Amplitude threshold for silence detection

        Returns:
            Trimmed audio
        """
        if len(audio) == 0:
            return audio

        # Find first non-silent sample
        abs_audio = np.abs(audio)
        non_silent = abs_audio > threshold

        if not non_silent.any():
            return np.array([], dtype=np.float32)

        start = non_silent.argmax()
        end = len(audio) - non_silent[::-1].argmax()

        # Add small padding
        pad_samples = int(0.05 * self.config.sample_rate)  # 50ms padding
        start = max(0, start - pad_samples)
        end = min(len(audio), end + pad_samples)

        return audio[start:end]

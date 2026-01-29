"""Faster-whisper inference engine."""

import logging
import time
from typing import Optional

import numpy as np

from .config import ModelConfig

logger = logging.getLogger(__name__)


class WhisperInference:
    """Faster-whisper model wrapper for speech-to-text inference."""

    def __init__(self, config: ModelConfig):
        self.config = config
        self.model = None
        self._load_time: float = 0.0

    def load(self) -> None:
        """Load the faster-whisper model."""
        from faster_whisper import WhisperModel

        logger.info(
            f"Loading model '{self.config.size}' on {self.config.device} "
            f"with compute_type={self.config.compute_type}"
        )

        start = time.perf_counter()

        device = self.config.device
        if device == "cuda":
            try:
                import torch
                if not torch.cuda.is_available():
                    logger.warning("CUDA not available, falling back to CPU")
                    device = "cpu"
            except ImportError:
                logger.warning("PyTorch not installed, using CPU")
                device = "cpu"

        self.model = WhisperModel(
            self.config.size,
            device=device,
            compute_type=self.config.compute_type,
            cpu_threads=self.config.num_threads,
        )

        self._load_time = time.perf_counter() - start
        logger.info(f"Model loaded in {self._load_time:.2f}s")

    def transcribe(
        self,
        audio: np.ndarray,
        language: Optional[str] = None,
    ) -> str:
        """
        Transcribe audio to text.

        Args:
            audio: Audio samples as float32 numpy array, shape (samples,)
            language: Override language (uses config.language if None)

        Returns:
            Transcribed text string
        """
        if self.model is None:
            raise RuntimeError("Model not loaded. Call load() first.")

        if len(audio) == 0:
            logger.warning("Empty audio buffer, skipping transcription")
            return ""

        # Ensure audio is float32 and normalized
        if audio.dtype != np.float32:
            audio = audio.astype(np.float32)

        # Normalize if needed (should be in range [-1, 1])
        max_val = np.abs(audio).max()
        if max_val > 1.0:
            audio = audio / max_val

        lang = language or self.config.language
        if lang == "auto":
            lang = None

        start = time.perf_counter()

        segments, info = self.model.transcribe(
            audio,
            language=lang,
            beam_size=self.config.beam_size,
            vad_filter=False,  # We handle VAD externally
            without_timestamps=True,
        )

        # Collect all segment texts
        text_parts = []
        for segment in segments:
            text_parts.append(segment.text)

        text = "".join(text_parts).strip()

        elapsed = time.perf_counter() - start
        audio_duration = len(audio) / 16000  # Assuming 16kHz
        rtf = elapsed / audio_duration if audio_duration > 0 else 0

        logger.debug(
            f"Transcription: {elapsed:.3f}s for {audio_duration:.2f}s audio "
            f"(RTF: {rtf:.2f}x realtime)"
        )

        return text

    def is_loaded(self) -> bool:
        """Check if model is loaded."""
        return self.model is not None

    @property
    def load_time(self) -> float:
        """Return model load time in seconds."""
        return self._load_time

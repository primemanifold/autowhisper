"""
Comprehensive pipeline performance tests for AutoWhisper.

These tests measure:
- Model loading time
- Transcription accuracy (Word Error Rate)
- Transcription latency (Real-Time Factor)
- Memory usage
- End-to-end pipeline performance

Run with:
    pytest tests/test_pipeline_performance.py -v
    pytest tests/test_pipeline_performance.py -v --benchmark-only  # benchmarks only
    pytest tests/test_pipeline_performance.py -v -s  # with output
"""

import gc
import os
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import numpy as np
import pytest

# Performance thresholds (adjust based on hardware)
# These are for RTX 5070 / modern GPU with distil-large-v3
THRESHOLDS = {
    "model_load_time_s": 5.0,  # Max seconds to load model
    "rtf_gpu": 0.5,  # Real-time factor for GPU (0.5 = 2x realtime)
    "rtf_cpu": 5.0,  # Real-time factor for CPU
    "word_error_rate": 0.15,  # Max 15% word error rate
    "warmup_runs": 2,  # Warmup runs before timing
    "benchmark_runs": 5,  # Number of benchmark runs
}


@dataclass
class BenchmarkResult:
    """Container for benchmark results."""
    name: str
    duration_s: float
    audio_duration_s: float
    rtf: float  # Real-time factor
    text: str
    device: str
    compute_type: str
    model_size: str
    
    def __str__(self) -> str:
        return (
            f"{self.name}:\n"
            f"  Duration: {self.duration_s:.3f}s\n"
            f"  Audio: {self.audio_duration_s:.2f}s\n"
            f"  RTF: {self.rtf:.3f}x realtime\n"
            f"  Device: {self.device} ({self.compute_type})\n"
            f"  Model: {self.model_size}\n"
            f"  Text: {self.text[:80]}{'...' if len(self.text) > 80 else ''}"
        )


def load_audio_file(path: Path) -> tuple[np.ndarray, int]:
    """
    Load audio file and return samples + sample rate.
    
    Supports: wav, mp3, ogg, flac via soundfile or pydub.
    """
    try:
        import soundfile as sf
        audio, sample_rate = sf.read(path)
        # Convert to mono if stereo
        if audio.ndim > 1:
            audio = audio.mean(axis=1)
        # Convert to float32
        audio = audio.astype(np.float32)
        return audio, sample_rate
    except Exception:
        pass
    
    # Fallback to pydub for ogg/other formats
    try:
        from pydub import AudioSegment
        sound = AudioSegment.from_file(str(path))
        # Convert to mono
        sound = sound.set_channels(1)
        # Get sample rate
        sample_rate = sound.frame_rate
        # Convert to numpy array
        samples = np.array(sound.get_array_of_samples(), dtype=np.float32)
        # Normalize to [-1, 1]
        samples = samples / (2 ** 15)
        return samples, sample_rate
    except Exception:
        pass
    
    # Final fallback: use ffmpeg via subprocess
    import subprocess
    import tempfile
    
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        tmp_path = tmp.name
    
    try:
        subprocess.run(
            ["ffmpeg", "-y", "-i", str(path), "-ar", "16000", "-ac", "1", "-f", "wav", tmp_path],
            capture_output=True,
            check=True,
        )
        import soundfile as sf
        audio, sample_rate = sf.read(tmp_path)
        return audio.astype(np.float32), sample_rate
    finally:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)


def resample_audio(audio: np.ndarray, orig_sr: int, target_sr: int = 16000) -> np.ndarray:
    """Resample audio to target sample rate (Whisper requires 16kHz)."""
    if orig_sr == target_sr:
        return audio
    
    # Simple linear interpolation resampling
    duration = len(audio) / orig_sr
    target_length = int(duration * target_sr)
    indices = np.linspace(0, len(audio) - 1, target_length)
    return np.interp(indices, np.arange(len(audio)), audio).astype(np.float32)


def calculate_wer(reference: str, hypothesis: str) -> float:
    """
    Calculate Word Error Rate (WER) between reference and hypothesis.
    
    WER = (S + D + I) / N
    Where: S=substitutions, D=deletions, I=insertions, N=words in reference
    """
    ref_words = reference.lower().strip().split()
    hyp_words = hypothesis.lower().strip().split()
    
    if not ref_words:
        return 0.0 if not hyp_words else 1.0
    
    # Dynamic programming for edit distance
    m, n = len(ref_words), len(hyp_words)
    dp = [[0] * (n + 1) for _ in range(m + 1)]
    
    for i in range(m + 1):
        dp[i][0] = i
    for j in range(n + 1):
        dp[0][j] = j
    
    for i in range(1, m + 1):
        for j in range(1, n + 1):
            if ref_words[i - 1] == hyp_words[j - 1]:
                dp[i][j] = dp[i - 1][j - 1]
            else:
                dp[i][j] = 1 + min(
                    dp[i - 1][j],      # deletion
                    dp[i][j - 1],      # insertion
                    dp[i - 1][j - 1],  # substitution
                )
    
    return dp[m][n] / m


class TestEnvironment:
    """Test environment and setup verification."""
    
    def test_torch_available(self):
        """Verify PyTorch is installed."""
        import torch
        assert torch is not None
        print(f"\nPyTorch version: {torch.__version__}")
    
    def test_cuda_available(self, cuda_available: bool, gpu_name: str, gpu_compute_capability: tuple):
        """Check CUDA availability and GPU info."""
        import torch
        
        print(f"\nCUDA available: {cuda_available}")
        if cuda_available:
            print(f"GPU: {gpu_name}")
            print(f"Compute capability: sm_{gpu_compute_capability[0]}{gpu_compute_capability[1]}")
            print(f"CUDA version: {torch.version.cuda}")
            print(f"Arch list: {torch.cuda.get_arch_list()}")
            
            # Check if GPU architecture is supported
            arch_list = torch.cuda.get_arch_list()
            sm_version = f"sm_{gpu_compute_capability[0]}{gpu_compute_capability[1]}"
            if sm_version not in arch_list:
                pytest.skip(
                    f"GPU architecture {sm_version} not in supported list: {arch_list}. "
                    "Consider upgrading PyTorch."
                )
    
    def test_faster_whisper_available(self):
        """Verify faster-whisper is installed."""
        import faster_whisper
        print(f"\nfaster-whisper version: {getattr(faster_whisper, '__version__', 'unknown')}")
    
    def test_ctranslate2_cuda_support(self, cuda_available: bool):
        """Check CTranslate2 CUDA support."""
        import ctranslate2
        
        print(f"\nCTranslate2 version: {ctranslate2.__version__}")
        cuda_count = ctranslate2.get_cuda_device_count()
        print(f"CTranslate2 CUDA devices: {cuda_count}")
        
        if cuda_available:
            supported_types = ctranslate2.get_supported_compute_types("cuda")
            print(f"Supported compute types (CUDA): {supported_types}")
    
    def test_audio_file_exists(self, test_audio_path: Path):
        """Verify test audio file exists and is readable."""
        assert test_audio_path.exists(), f"Audio file not found: {test_audio_path}"
        print(f"\nTest audio: {test_audio_path.name}")
        print(f"File size: {test_audio_path.stat().st_size / 1024:.1f} KB")


class TestModelLoading:
    """Test model loading performance."""
    
    @pytest.fixture(scope="class")
    def model_sizes(self) -> list[str]:
        """Model sizes to test."""
        # Only test distil-large-v3 by default (fastest to download/load)
        return ["distil-large-v3"]
    
    def test_model_load_cuda(self, cuda_available: bool):
        """Benchmark model loading on CUDA."""
        if not cuda_available:
            pytest.skip("CUDA not available")
        
        from faster_whisper import WhisperModel
        
        # Force garbage collection before loading
        gc.collect()
        
        start = time.perf_counter()
        model = WhisperModel(
            "distil-large-v3",
            device="cuda",
            compute_type="float16",
        )
        load_time = time.perf_counter() - start
        
        print(f"\nModel load time (CUDA, float16): {load_time:.2f}s")
        assert load_time < THRESHOLDS["model_load_time_s"], (
            f"Model load time {load_time:.2f}s exceeds threshold "
            f"{THRESHOLDS['model_load_time_s']}s"
        )
        
        del model
        gc.collect()
    
    def test_model_load_cpu(self):
        """Benchmark model loading on CPU."""
        from faster_whisper import WhisperModel
        
        gc.collect()
        
        start = time.perf_counter()
        model = WhisperModel(
            "distil-large-v3",
            device="cpu",
            compute_type="float32",
        )
        load_time = time.perf_counter() - start
        
        print(f"\nModel load time (CPU, float32): {load_time:.2f}s")
        # CPU loading is slower, use relaxed threshold
        assert load_time < THRESHOLDS["model_load_time_s"] * 3
        
        del model
        gc.collect()


class TestTranscriptionAccuracy:
    """Test transcription accuracy with known audio."""
    
    @pytest.fixture(scope="class")
    def whisper_model(self, cuda_available: bool):
        """Load model once for all accuracy tests."""
        from faster_whisper import WhisperModel
        
        device = "cuda" if cuda_available else "cpu"
        compute_type = "float16" if cuda_available else "float32"
        
        model = WhisperModel(
            "distil-large-v3",
            device=device,
            compute_type=compute_type,
        )
        yield model
        del model
        gc.collect()
    
    @pytest.fixture(scope="class")
    def test_audio(self, test_audio_path: Path) -> np.ndarray:
        """Load and prepare test audio."""
        audio, sample_rate = load_audio_file(test_audio_path)
        # Resample to 16kHz if needed
        audio = resample_audio(audio, sample_rate, 16000)
        return audio
    
    def test_pangram_transcription(
        self,
        whisper_model,
        test_audio: np.ndarray,
        expected_transcription: str,
    ):
        """Test transcription of pangram audio for accuracy."""
        segments, info = whisper_model.transcribe(
            test_audio,
            language="en",
            beam_size=5,  # Higher beam size for accuracy
            without_timestamps=True,
        )
        
        text = "".join([s.text for s in segments]).strip().lower()
        wer = calculate_wer(expected_transcription, text)
        
        print(f"\nExpected: {expected_transcription}")
        print(f"Got: {text}")
        print(f"Word Error Rate: {wer:.2%}")
        
        assert wer <= THRESHOLDS["word_error_rate"], (
            f"WER {wer:.2%} exceeds threshold {THRESHOLDS['word_error_rate']:.0%}"
        )
    
    def test_empty_audio_handling(self, whisper_model):
        """Test handling of empty/silent audio."""
        # 1 second of silence
        silent_audio = np.zeros(16000, dtype=np.float32)
        
        segments, info = whisper_model.transcribe(
            silent_audio,
            language="en",
            without_timestamps=True,
        )
        
        text = "".join([s.text for s in segments]).strip()
        print(f"\nSilent audio transcription: '{text}'")
        # Should either be empty or have minimal hallucination
        assert len(text) < 50, f"Too much hallucination on silence: {text}"
    
    def test_very_short_audio(self, whisper_model):
        """Test handling of very short audio (< 1s)."""
        # 0.5 seconds of audio (just noise)
        short_audio = np.random.randn(8000).astype(np.float32) * 0.01
        
        segments, info = whisper_model.transcribe(
            short_audio,
            language="en",
            without_timestamps=True,
        )
        
        text = "".join([s.text for s in segments]).strip()
        print(f"\nShort audio transcription: '{text}'")
        # Should handle gracefully without crashing


class TestTranscriptionPerformance:
    """Test transcription performance and latency."""
    
    @pytest.fixture(scope="class")
    def whisper_model_cuda(self, cuda_available: bool):
        """Load CUDA model for performance tests."""
        if not cuda_available:
            pytest.skip("CUDA not available")
        
        from faster_whisper import WhisperModel
        
        model = WhisperModel(
            "distil-large-v3",
            device="cuda",
            compute_type="float16",
        )
        yield model
        del model
        gc.collect()
    
    @pytest.fixture(scope="class")
    def test_audio(self, test_audio_path: Path) -> np.ndarray:
        """Load and prepare test audio."""
        audio, sample_rate = load_audio_file(test_audio_path)
        audio = resample_audio(audio, sample_rate, 16000)
        return audio
    
    def test_cuda_transcription_rtf(
        self,
        whisper_model_cuda,
        test_audio: np.ndarray,
    ):
        """Benchmark CUDA transcription Real-Time Factor."""
        audio_duration = len(test_audio) / 16000
        
        # Warmup runs
        for _ in range(THRESHOLDS["warmup_runs"]):
            segments, _ = whisper_model_cuda.transcribe(
                test_audio,
                language="en",
                beam_size=1,
                without_timestamps=True,
            )
            _ = "".join([s.text for s in segments])
        
        # Benchmark runs
        durations = []
        for i in range(THRESHOLDS["benchmark_runs"]):
            start = time.perf_counter()
            segments, _ = whisper_model_cuda.transcribe(
                test_audio,
                language="en",
                beam_size=1,
                without_timestamps=True,
            )
            text = "".join([s.text for s in segments])
            elapsed = time.perf_counter() - start
            durations.append(elapsed)
        
        mean_duration = np.mean(durations)
        std_duration = np.std(durations)
        rtf = mean_duration / audio_duration
        
        result = BenchmarkResult(
            name="CUDA Transcription (beam=1)",
            duration_s=mean_duration,
            audio_duration_s=audio_duration,
            rtf=rtf,
            text=text,
            device="cuda",
            compute_type="float16",
            model_size="distil-large-v3",
        )
        
        print(f"\n{result}")
        print(f"  Std dev: {std_duration:.3f}s")
        print(f"  Min: {min(durations):.3f}s, Max: {max(durations):.3f}s")
        
        assert rtf < THRESHOLDS["rtf_gpu"], (
            f"RTF {rtf:.2f}x exceeds GPU threshold {THRESHOLDS['rtf_gpu']}x"
        )
    
    def test_cuda_transcription_beam5(
        self,
        whisper_model_cuda,
        test_audio: np.ndarray,
    ):
        """Benchmark CUDA transcription with beam=5 (more accurate, slower)."""
        audio_duration = len(test_audio) / 16000
        
        # Warmup
        segments, _ = whisper_model_cuda.transcribe(
            test_audio,
            language="en",
            beam_size=5,
            without_timestamps=True,
        )
        _ = "".join([s.text for s in segments])
        
        # Benchmark
        start = time.perf_counter()
        segments, _ = whisper_model_cuda.transcribe(
            test_audio,
            language="en",
            beam_size=5,
            without_timestamps=True,
        )
        text = "".join([s.text for s in segments])
        elapsed = time.perf_counter() - start
        
        rtf = elapsed / audio_duration
        
        result = BenchmarkResult(
            name="CUDA Transcription (beam=5)",
            duration_s=elapsed,
            audio_duration_s=audio_duration,
            rtf=rtf,
            text=text,
            device="cuda",
            compute_type="float16",
            model_size="distil-large-v3",
        )
        
        print(f"\n{result}")
        
        # Beam=5 is slower, use relaxed threshold
        assert rtf < THRESHOLDS["rtf_gpu"] * 3
    
    def test_memory_usage(self, whisper_model_cuda, test_audio: np.ndarray, cuda_available: bool):
        """Monitor GPU memory usage during transcription."""
        if not cuda_available:
            pytest.skip("CUDA not available")
        
        import torch
        
        # Clear cache
        torch.cuda.empty_cache()
        
        mem_before = torch.cuda.memory_allocated() / (1024 ** 3)
        
        # Run transcription
        segments, _ = whisper_model_cuda.transcribe(
            test_audio,
            language="en",
            beam_size=1,
            without_timestamps=True,
        )
        _ = "".join([s.text for s in segments])
        
        mem_after = torch.cuda.memory_allocated() / (1024 ** 3)
        mem_peak = torch.cuda.max_memory_allocated() / (1024 ** 3)
        
        print(f"\nGPU Memory:")
        print(f"  Before: {mem_before:.2f} GB")
        print(f"  After: {mem_after:.2f} GB")
        print(f"  Peak: {mem_peak:.2f} GB")
        print(f"  Delta: {mem_after - mem_before:.3f} GB")


class TestEndToEndPipeline:
    """Test the complete AutoWhisper pipeline."""
    
    def test_config_loading(self, config_path: Path):
        """Test configuration loading."""
        from autowhisper.config import Config
        
        config = Config.load(config_path)
        
        print(f"\nConfig loaded:")
        print(f"  Model: {config.model.size}")
        print(f"  Device: {config.model.device}")
        print(f"  Compute type: {config.model.compute_type}")
        print(f"  Beam size: {config.model.beam_size}")
        
        assert config.model.size == "distil-large-v3"
    
    def test_inference_module(self, config_path: Path, test_audio_path: Path, cuda_available: bool):
        """Test the inference module directly."""
        from autowhisper.config import Config
        from autowhisper.inference import WhisperInference
        
        config = Config.load(config_path)
        
        # Override device based on availability
        if not cuda_available:
            config.model.device = "cpu"
            config.model.compute_type = "float32"
        
        # Create inference engine
        inference = WhisperInference(config.model)
        
        # Time model loading
        start = time.perf_counter()
        inference.load()
        load_time = time.perf_counter() - start
        
        print(f"\nInference module:")
        print(f"  Load time: {load_time:.2f}s")
        print(f"  Is loaded: {inference.is_loaded()}")
        
        # Load and transcribe test audio
        audio, sample_rate = load_audio_file(test_audio_path)
        audio = resample_audio(audio, sample_rate, 16000)
        
        audio_duration = len(audio) / 16000
        
        start = time.perf_counter()
        text = inference.transcribe(audio)
        transcribe_time = time.perf_counter() - start
        
        rtf = transcribe_time / audio_duration
        
        print(f"  Transcribe time: {transcribe_time:.2f}s")
        print(f"  Audio duration: {audio_duration:.2f}s")
        print(f"  RTF: {rtf:.2f}x realtime")
        print(f"  Text: {text}")
        
        assert len(text) > 0, "Transcription should not be empty"
    
    def test_audio_manager(self, config_path: Path):
        """Test audio manager initialization."""
        from autowhisper.config import Config
        from autowhisper.audio import AudioManager
        
        config = Config.load(config_path)
        
        # Disable VAD for this test (may not have silero)
        config.audio.vad_enabled = False
        
        audio_mgr = AudioManager(config.audio)
        audio_mgr.initialize()
        
        print(f"\nAudio manager initialized")
        print(f"  Sample rate: {config.audio.sample_rate}")
        print(f"  Buffer size: {config.audio.buffer_size}")
    
    def test_feedback_manager(self, config_path: Path):
        """Test feedback manager (beeps)."""
        from autowhisper.config import Config
        from autowhisper.feedback import FeedbackManager
        
        config = Config.load(config_path)
        
        feedback = FeedbackManager(config.feedback)
        
        # Test tone generation (don't actually play in CI)
        tone = feedback._generate_tone(800, 0.1, 0.3)
        
        print(f"\nFeedback manager:")
        print(f"  Tone samples: {len(tone)}")
        print(f"  Tone dtype: {tone.dtype}")
        
        assert len(tone) > 0


class TestComputeTypes:
    """Test different compute types for compatibility."""
    
    @pytest.mark.parametrize("compute_type", ["float16", "float32"])
    def test_compute_type_cuda(self, compute_type: str, cuda_available: bool, test_audio_path: Path):
        """Test different compute types on CUDA."""
        if not cuda_available:
            pytest.skip("CUDA not available")
        
        from faster_whisper import WhisperModel
        import ctranslate2
        
        # Check if compute type is supported
        supported = ctranslate2.get_supported_compute_types("cuda")
        if compute_type not in supported:
            pytest.skip(f"Compute type {compute_type} not supported. Available: {supported}")
        
        model = WhisperModel(
            "distil-large-v3",
            device="cuda",
            compute_type=compute_type,
        )
        
        # Load test audio
        audio, sample_rate = load_audio_file(test_audio_path)
        audio = resample_audio(audio, sample_rate, 16000)
        
        # Transcribe
        start = time.perf_counter()
        segments, _ = model.transcribe(audio, language="en", beam_size=1, without_timestamps=True)
        text = "".join([s.text for s in segments])
        elapsed = time.perf_counter() - start
        
        audio_duration = len(audio) / 16000
        rtf = elapsed / audio_duration
        
        print(f"\nCompute type: {compute_type}")
        print(f"  RTF: {rtf:.2f}x realtime")
        print(f"  Text: {text[:60]}...")
        
        del model
        gc.collect()


# Benchmark fixtures for pytest-benchmark
@pytest.fixture
def benchmark_audio(test_audio_path: Path) -> np.ndarray:
    """Load audio for benchmarking."""
    audio, sample_rate = load_audio_file(test_audio_path)
    return resample_audio(audio, sample_rate, 16000)


@pytest.fixture(scope="module")
def benchmark_model(cuda_available: bool):
    """Load model for benchmarking."""
    from faster_whisper import WhisperModel
    
    device = "cuda" if cuda_available else "cpu"
    compute_type = "float16" if cuda_available else "float32"
    
    model = WhisperModel("distil-large-v3", device=device, compute_type=compute_type)
    yield model
    del model
    gc.collect()


class TestBenchmarks:
    """Benchmark tests using pytest-benchmark."""
    
    @pytest.mark.benchmark(group="transcription")
    def test_benchmark_transcription(self, benchmark, benchmark_model, benchmark_audio: np.ndarray):
        """Benchmark transcription with pytest-benchmark."""
        def transcribe():
            segments, _ = benchmark_model.transcribe(
                benchmark_audio,
                language="en",
                beam_size=1,
                without_timestamps=True,
            )
            return "".join([s.text for s in segments])
        
        result = benchmark(transcribe)
        
        audio_duration = len(benchmark_audio) / 16000
        
        # Note: benchmark stats are printed in the pytest-benchmark table
        # Mean time shown there can be used to calculate RTF manually
        print(f"\nAudio duration: {audio_duration:.2f}s")
        print(f"Result: {result[:60]}...")
        print("(See benchmark table above for timing stats)")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])

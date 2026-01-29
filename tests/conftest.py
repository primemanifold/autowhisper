"""Pytest configuration and shared fixtures."""

import os
from pathlib import Path

import pytest


@pytest.fixture(scope="session")
def project_root() -> Path:
    """Return the project root directory."""
    return Path(__file__).parent.parent


@pytest.fixture(scope="session")
def test_audio_path(project_root: Path) -> Path:
    """Return the path to the test audio file."""
    audio_path = project_root / "test" / "Audio_Sample_-_The_Quick_Brown_Fox_Jumps_Over_The_Lazy_Dog.ogg"
    if not audio_path.exists():
        pytest.skip(f"Test audio file not found: {audio_path}")
    return audio_path


@pytest.fixture(scope="session")
def expected_transcription() -> str:
    """Return the expected transcription of the test audio."""
    # The pangram: "The quick brown fox jumps over the lazy dog"
    return "the quick brown fox jumps over the lazy dog"


@pytest.fixture(scope="session")
def config_path(project_root: Path) -> Path:
    """Return the path to the config file."""
    return project_root / "config.toml"


@pytest.fixture(scope="module")
def cuda_available() -> bool:
    """Check if CUDA is available."""
    try:
        import torch
        return torch.cuda.is_available()
    except ImportError:
        return False


@pytest.fixture(scope="module")
def gpu_name() -> str:
    """Return GPU name if available."""
    try:
        import torch
        if torch.cuda.is_available():
            return torch.cuda.get_device_name(0)
    except Exception:
        pass
    return "N/A"


@pytest.fixture(scope="module")
def gpu_compute_capability() -> tuple:
    """Return GPU compute capability if available."""
    try:
        import torch
        if torch.cuda.is_available():
            return torch.cuda.get_device_capability(0)
    except Exception:
        pass
    return (0, 0)

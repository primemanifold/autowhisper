# Testing Guide for AutoWhisper

This guide covers how to test AutoWhisper comprehensively.

## Quick Test Summary

```bash
# Run all unit tests
python -m pytest

# Run tests with verbose output
python -m pytest -v

# Run specific test module
python -m pytest tests/test_config.py

# Run with coverage
python -m pytest --cov=autowhisper
```

## Manual Testing Checklist

### 1. Configuration Testing

```bash
# Test valid config
python -m autowhisper --config config.toml

# Test invalid config (should fail gracefully)
cp config.toml test-config.toml
# Edit test-config.toml with invalid values
python -m autowhisper --config test-config.toml
```

**Test Cases:**
- [ ] Valid config loads successfully
- [ ] Invalid model size shows error
- [ ] Invalid device shows error
- [ ] Invalid hotkey mode shows error
- [ ] Invalid output method shows error
- [ ] Missing config file shows error

### 2. Audio Capture Testing

```bash
# Test microphone detection
arecord -l  # List audio devices

# Test recording
arecord -d 3 -f S16_LE -r 16000 -c 1 test.wav
aplay test.wav  # Verify recording works
```

**Test Cases:**
- [ ] Default microphone is detected
- [ ] Custom device name works
- [ ] Recording starts on hotkey press
- [ ] Recording stops on hotkey release
- [ ] Audio buffer fills correctly
- [ ] VAD trims silence correctly

### 3. Hotkey Testing

```bash
# Run with debug logging
python -m autowhisper --config config.toml --log-level debug

# Test hotkey registration
# Press configured hotkey and check logs
```

**Test Cases:**
- [ ] Hotkey registration succeeds
- [ ] Push-to-talk mode works (hold to record)
- [ ] Toggle mode works (press to start/stop)
- [ ] Cancel hotkey aborts recording
- [ ] Multiple hotkey presses handled correctly
- [ ] Hotkey works in different applications

### 4. Inference Testing

```bash
# Test with sample audio file
# Create test audio first:
arecord -d 5 -f S16_LE -r 16000 -c 1 test.wav

# Or use existing audio file
# The inference should handle various audio inputs
```

**Test Cases:**
- [ ] Empty audio returns empty result
- [ ] Short audio (< 1s) transcribes correctly
- [ ] Long audio (> 10s) transcribes correctly
- [ ] CUDA inference uses GPU
- [ ] CPU fallback works if CUDA unavailable
- [ ] Different languages work (if configured)

### 5. Output Testing

```bash
# Test clipboard mode
# Edit config.toml:
# [output]
# method = "clipboard"
# auto_paste = true

# Test inject mode (default)
# [output]
# method = "inject"
```

**Test Cases:**
- [ ] Text appears in active window (inject mode)
- [ ] Text copied to clipboard (clipboard mode)
- [ ] Auto-paste works (clipboard + auto_paste)
- [ ] Lowercase conversion works
- [ ] Newline appending works
- [ ] Special characters handled correctly

### 6. Feedback Testing

```bash
# Test audio feedback
# Ensure speakers/headphones are connected
```

**Test Cases:**
- [ ] Start beep plays on recording start
- [ ] Stop beep plays on recording stop
- [ ] Error beep plays on errors
- [ ] Beep volume is adjustable
- [ ] Beeps can be disabled

### 7. Integration Testing

```bash
# Full end-to-end test
# 1. Start daemon
python -m autowhisper --config config.toml

# 2. Open text editor (gedit, vim, etc.)

# 3. Press and hold hotkey

# 4. Speak clearly: "Hello world"

# 5. Release hotkey

# 6. Verify text appears in editor
```

**Test Cases:**
- [ ] Complete workflow: hotkey → record → transcribe → output
- [ ] Works in different applications (browser, editor, terminal)
- [ ] Multiple recordings in sequence work
- [ ] Error recovery (e.g., GPU unavailable)
- [ ] Daemon restarts correctly after errors

### 8. Performance Testing

```bash
# Monitor GPU usage
watch -n 0.1 nvidia-smi

# Monitor CPU usage
htop

# Test latency
# Time from hotkey release to text appearance
```

**Test Cases:**
- [ ] Latency < 200ms for 5s audio (RTX 5070)
- [ ] GPU utilization during inference
- [ ] Memory usage stays reasonable
- [ ] No memory leaks over time
- [ ] CPU usage low when idle

### 9. Error Handling Testing

```bash
# Test various error conditions
```

**Test Cases:**
- [ ] Missing model file shows error
- [ ] No microphone shows error
- [ ] X11 unavailable (for inject mode) falls back to xdotool
- [ ] GPU unavailable falls back to CPU
- [ ] Invalid audio format handled gracefully

### 10. Daemon Testing

```bash
# Test as systemd service
sudo systemctl start autowhisper@$USER
sudo systemctl status autowhisper@$USER
journalctl -u autowhisper@$USER -f

# Test foreground mode
python -m autowhisper --config config.toml
```

**Test Cases:**
- [ ] Daemon starts successfully
- [ ] Daemon runs in background
- [ ] Logs appear in journald
- [ ] PID file created correctly
- [ ] Multiple instances prevented
- [ ] Graceful shutdown on SIGTERM

## Automated Test Script

Create a test script for quick validation:

```bash
#!/bin/bash
# test-autowhisper.sh

set -e

echo "Running unit tests..."
python -m pytest -v

echo "Checking config..."
python -m autowhisper --config config.toml --help || true

echo "All automated tests passed!"
```

## Continuous Integration

For CI/CD, add to `.github/workflows/test.yml`:

```yaml
name: Test

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.12'
      - run: pip install -e ".[dev]"
      - run: python -m pytest -v
      - run: python -m ruff check src/
```

## Debugging Failed Tests

```bash
# Run with verbose output
python -m pytest -v --tb=long

# Run single test
python -m pytest tests/test_config.py::test_valid_config

# Run with print output
python -m pytest -s

# Debug with pdb
python -m pytest --pdb
```

## Adding New Tests

When adding new features, add corresponding tests:

1. **Unit tests** for pure functions
2. **Integration tests** for component interactions
3. **Manual test cases** for user-facing features

Example:

```python
import pytest
from autowhisper.config import load_config

def test_valid_config():
    """Test that valid config loads successfully."""
    config = load_config("config.toml")
    assert config.model.size == "distil-large-v3"
    assert config.model.device == "cuda"

def test_invalid_model_size():
    """Test that invalid model size raises error."""
    with pytest.raises(ValueError):
        # Test with invalid config
        pass
```

## Performance Benchmarks

For performance-critical code, add benchmarks:

```python
import pytest
import time

def test_transcription_performance(benchmark):
    """Benchmark transcription speed."""
    # Setup
    audio = load_test_audio()
    
    # Benchmark
    result = benchmark(transcribe, audio)
    
    # Assert performance
    assert benchmark.stats.mean < 0.5  # < 500ms
```

Run with: `python -m pytest --benchmark-only`

## Test Coverage

Generate coverage report:

```bash
# Install pytest-cov
pip install pytest-cov

# Generate coverage
python -m pytest --cov=autowhisper --cov-report=html

# View report
open htmlcov/index.html
```

## Next Steps

1. Add integration tests for daemon workflow
2. Add performance benchmarks
3. Add end-to-end test script
4. Set up CI/CD pipeline

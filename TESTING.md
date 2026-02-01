# Testing Guide

## Quick Start

```bash
python -m pytest -v              # Run all tests
python -m pytest --cov=autowhisper  # With coverage
```

## Manual Testing Checklist

### Audio
- [ ] Default microphone detected
- [ ] Recording starts/stops on hotkey
- [ ] VAD trims silence correctly

### Hotkeys
- [ ] Push-to-talk mode works (hold to record)
- [ ] Toggle mode works (press to start/stop)
- [ ] Cancel hotkey aborts recording

### Output
- [ ] Text appears in active window (inject mode)
- [ ] Text copied to clipboard (clipboard mode)
- [ ] Special characters handled correctly

### End-to-End
```bash
python -m autowhisper --config config.toml
# Open text editor, press Shift+Super, speak "Hello world", release
# Verify text appears
```

## Performance Testing

```bash
watch -n 0.1 nvidia-smi  # Monitor GPU during transcription
```

Target: < 200ms latency for 5s audio on RTX 5070

## Debug Mode

```bash
python -m autowhisper --config config.toml --log-level debug
```

# Faster-Whisper Migration Design — Review Notes (2026-01-29)

This document captures review feedback for `docs/plans/2026-01-29-faster-whisper-migration-design.md`.

## Overall

The target architecture is sensible (Python orchestration + CTranslate2 inference), but a few details will materially affect reliability and whether the latency targets are achievable in practice:

- CUDA enablement and detection (don’t rely on PyTorch being installed)
- Audio callback safety (avoid heavy work in the sounddevice callback)
- Desktop environment assumptions (X11 vs Wayland)
- Concurrency semantics (what happens on overlapping hotkeys / cancellations)

## High-Impact Corrections

### 1) “RTF” naming/values are confusing

The table uses “Expected RTF ~25x/~100x”. “RTF” commonly means *real-time factor* where **lower is better** (e.g., `0.04`), while “x” implies *speedup vs realtime* where **higher is better** (e.g., `25x`).

Recommendation:
- Rename the row to **“Speed vs realtime”** if you want `25x/100x`, or
- Keep **RTF** but express as `0.04 → 0.01` (example values).

### 2) CUDA availability check should not depend on `torch`

The design’s error-handling suggests `torch.cuda.is_available()` to decide CUDA fallback. That will mis-detect if PyTorch is not installed (which is likely if you only use faster-whisper/CTranslate2).

Recommendation:
- Treat `device="cuda"` as desired and fall back to CPU on model init failure, or
- Detect CUDA via `ctranslate2.get_cuda_device_count()` (or equivalent) instead of PyTorch.

### 3) Ensure the install actually gets CUDA-enabled CTranslate2

The difference between “works” and “hits performance targets” is often whether you end up with a CUDA build of `ctranslate2`.

Recommendation:
- Add an explicit “GPU validation” step: log `ctranslate2.get_cuda_device_count()` at startup and warn loudly if 0 when `device=cuda`.
- Document the exact install path for CUDA wheels for your target platform (and how to verify it).

### 4) Model identifier / cache control needs a plan

The config uses `size = "distil-large-v3"`. Depending on faster-whisper version, this may or may not map cleanly without a full HF identifier.

Recommendation:
- Add a config option for `model_path` or `model_cache_dir`, and clarify the canonical model id you expect to use.

## Reliability & Latency Risks

### 1) Avoid running Silero VAD (Torch) inside the audio callback

Calling a neural VAD inside the `sounddevice` callback risks buffer overruns/dropouts due to Python/Torch latency and GIL interactions.

Recommendation:
- Keep the callback minimal: copy audio into a ring buffer / queue only.
- Run VAD + trimming + “stop on silence” logic in a separate worker thread that consumes audio chunks.
- Alternatively, consider faster-whisper’s built-in `vad_filter` if it meets your needs.

### 2) Define concurrency semantics (Recording/Processing overlap)

The “main thread daemon loop” suggests inference happens inline, but doesn’t specify how new START/STOP/CANCEL events behave during Processing.

Recommendation:
- Define a policy: drop events while Processing, queue them, or cancel current inference.
- If you want cancel to work reliably, inference should run in its own worker thread/process with a cancellation mechanism.

### 3) Desktop environment: X11 vs Wayland

`python-xlib` and `xdotool` are X11-focused; Wayland often blocks injection. The design should explicitly state supported environments and fallback behavior.

Recommendation:
- Document: “X11 supported; Wayland uses clipboard-only (or requires user to enable XWayland / compositor settings).”

## Config/Defaults Suggestions

- Consider `device = "auto"` as the default with clear logging about what was chosen.
- Compute type may need to vary by device; document CPU-friendly alternatives if CUDA isn’t available.
- The inference code assumes `16000` Hz; ensure audio path either enforces resampling or validates sample rate.

## Suggested Next Steps

1. Update the design doc to reflect the corrected CUDA detection and RTF wording.
2. Add an explicit “GPU validation” startup check + log line (for quick field debugging).
3. Decide and document the Wayland/X11 support policy and output fallbacks.
4. Move VAD processing out of the sounddevice callback in the implementation plan.


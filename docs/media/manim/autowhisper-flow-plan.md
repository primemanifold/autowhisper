# AutoWhisper Flow Explainer — Manim Plan

This is a plan only. No Manim render has been generated in this repository.

## Narrative arc

1. Voice begins on device.
2. Audio stays local-first and offline where platform support is proven.
3. Whisper produces text on supported desktop paths.
4. Platform readiness is shown honestly: macOS, Linux, Windows, and iOS have different proof levels.
5. The user sees what is shipped now and what remains planned.

## Scene list

### Scene 1 — Local microphone to local model

Visual: microphone icon, waveform, local model box, text output.

Caption: "Local-first voice-to-text. No cloud account is needed for supported desktop paths."

### Scene 2 — Platform readiness ladder

Visual: four columns for macOS, Linux, Windows, iOS.

Caption: "Proof levels are separated: build, package, install, runtime, and end-to-end."

### Scene 3 — macOS/Linux shipped paths

Visual: macOS app ZIP and Linux PPA commands.

Caption: "macOS v0.7.1 public ZIP and Linux PPA path are documented; runtime proof remains explicit."

### Scene 4 — Windows/iOS boundaries

Visual: warning labels for Windows runtime unproven and iOS whisper.cpp bridge pending.

Caption: "No Windows runtime claim from cross-build alone. No iOS local transcription until the bridge outputs real text."

### Scene 5 — Evidence-first roadmap

Visual: validation checklist fills in as tests pass.

Caption: "Every platform claim needs evidence."

## Visual design notes

Use the warm minimal AutoWhisper design language: off-white background, dark ink text, blue primary accent, amber caution badges, green evidence badges.

## Intended output path

`docs/media/generated/autowhisper-flow-explainer.mp4`

## Future render command

```bash
manim -pqh docs/media/manim/autowhisper-flow.py AutoWhisperFlow
```

Do not run this until a real Manim scene file is approved.

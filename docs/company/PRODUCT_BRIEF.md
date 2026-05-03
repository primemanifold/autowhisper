# AutoWhisper Product Brief

Last updated: 2026-05-03

## One-line description

AutoWhisper is a local-first voice transcription and dictation product for turning spoken thought into useful text with low friction and strong privacy boundaries.

## Current product reality

Evidence from the repo shows three active product surfaces:

1. **Linux desktop dictation** — the original C++ app targets Ubuntu/X11 with hotkey capture, microphone recording, whisper.cpp inference, and X11/clipboard output. Evidence: `README.md`, `src/main.cpp`, `src/daemon/daemon.cpp`, `src/audio/audio.cpp`, `src/inference/inference.cpp`, `src/hotkey/platform/hotkey_x11.cpp`, `src/output/platform/output_x11.cpp`.
2. **macOS app path** — macOS code and release packaging exist, including a menu-bar bundle, onboarding/settings work, and native platform implementations. Evidence: `src/platform/macos/onboarding.mm`, `src/hotkey/platform/hotkey_macos.mm`, `src/output/platform/output_macos.mm`, `cmake/MacOSBundle.cmake`, `platform/macos/SettingsApp.swift`.
3. **iOS foundation** — a SwiftUI iOS shell records foreground audio, decodes PCM, has a placeholder transcriber seam, model-resource locator, and a widget/deep-link launcher. Evidence: `ios/AutoWhisperApp/ContentView.swift`, `ios/AutoWhisperApp/IOSAudioRecorder.swift`, `ios/AutoWhisperApp/IOSAudioDecoder.swift`, `ios/AutoWhisperWidget/AutoWhisperWidget.swift`.

## Primary user hypotheses

- Privacy-sensitive power users who want local transcription instead of cloud dictation.
- Developers/operators who want voice input that can become structured work artifacts.
- Mac users who want a low-friction alternative to generic dictation, but only after macOS first-run and release artifacts are consistently proven.
- Accessibility users who need reliable dictation, but this must be treated as a serious quality bar rather than marketing copy.

## Current stage

Pre-1.0 hardening. The strongest near-term opportunity is to make the first successful dictation experience reliable, evidence-backed, and easy to explain before expanding claims.

## Non-goals for now

- Do not claim iOS production readiness; the real Whisper bridge is still pending.
- Do not claim Windows runtime support; cross-build proof is not runtime proof.
- Do not compete with meeting-note products until the core capture/transcription/insertion loop and library/storage story are explicit.

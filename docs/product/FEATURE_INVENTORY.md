# Feature Inventory

Last updated: 2026-05-03

## Implemented or partially implemented

| Area | Status | Evidence |
|---|---|---|
| Linux hotkey dictation | Implemented for X11 path | `src/hotkey/platform/hotkey_x11.cpp`, `src/output/platform/output_x11.cpp`, `README.md` |
| Audio capture | Implemented in C++ desktop path | `src/audio/audio.cpp` |
| Local inference | Implemented via whisper.cpp integration | `src/inference/inference.cpp`, `deps/whisper.cpp` |
| Settings UI | Implemented as embedded web UI plus native macOS settings helper | `src/settings/web/`, `src/settings/handlers.cpp`, `platform/macos/SettingsApp.swift` |
| Linux packaging | Present | `packaging/` files and GitHub Actions build job |
| macOS bundle/release path | In progress | `cmake/MacOSBundle.cmake`, `platform/macos/Info.plist.in`, macOS platform files |
| iOS app shell | Foundation only | `ios/AutoWhisperApp/` |
| iOS widget launcher | Foundation only; opens app | `ios/AutoWhisperWidget/` |
| Windows support | Build/cross-build evidence only; runtime unproven | Windows platform files and readiness docs |

## Not yet product-ready

- iOS real local Whisper transcription.
- Physical iOS device E2E proof.
- Windows runtime proof.
- Meeting transcription/library workflow.
- Speaker diarization.
- Personal dictionary/custom vocabulary UX.
- Reproducible latency/accuracy benchmark suite.

# Architecture

Last updated: 2026-05-03

## Tech stack

- Desktop core: C++20 with CMake.
- Inference: `whisper.cpp` vendored under `deps/whisper.cpp`.
- Audio: desktop C++ capture in `src/audio/audio.cpp`; iOS AVFoundation recorder in `ios/AutoWhisperApp/IOSAudioRecorder.swift`.
- Settings: embedded web UI in `src/settings/web/` and native macOS helper in `platform/macos/SettingsApp.swift`.
- iOS: Swift Package plus XcodeGen project under `ios/`.
- Tests: Catch2/CTest for C++, Python static tests under `tests/static`, Swift package checks under `ios/Sources/AutoWhisperCoreChecks`.
- CI: GitHub Actions in `.github/workflows/ci.yml`, including Ubuntu build and macOS iOS build job.

## Desktop flow

Hotkey -> audio capture -> optional trimming/prep -> whisper.cpp inference -> output injection/clipboard -> feedback/logging.

Key files:
- `src/main.cpp`
- `src/daemon/daemon.cpp`
- `src/audio/audio.cpp`
- `src/inference/inference.cpp`
- `src/hotkey/platform/`
- `src/output/platform/`
- `src/config/`

## iOS flow

SwiftUI view model -> microphone permission check -> AVFoundation foreground recording -> CAF/PCM decode -> placeholder transcriber seam -> transcript placeholder. Widget/deep link opens the foreground app; it does not record in the widget.

Key files:
- `ios/AutoWhisperApp/ContentView.swift`
- `ios/AutoWhisperApp/IOSAudioRecorder.swift`
- `ios/AutoWhisperApp/IOSAudioDecoder.swift`
- `ios/AutoWhisperApp/IOSWhisperTranscriber.swift`
- `ios/AutoWhisperWidget/`

## Platform support truth

- Linux: strongest implemented runtime path, especially X11.
- macOS: app bundle/release path exists and is being hardened; validate public artifacts separately.
- Windows: build/cross-build path exists, runtime remains unproven.
- iOS: app shell and CI build proof exist; real local transcription and physical-device proof remain pending.

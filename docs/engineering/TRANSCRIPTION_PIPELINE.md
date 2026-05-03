# Transcription Pipeline

Last updated: 2026-05-03

## Desktop pipeline

1. User triggers hotkey.
2. Audio is captured locally.
3. Audio is prepared for model inference.
4. `whisper.cpp` runs local transcription.
5. Text is inserted through platform output or copied to clipboard.
6. Feedback/logging surfaces status.

Evidence: `src/audio/audio.cpp`, `src/inference/inference.cpp`, `src/output/platform/`, `src/feedback/feedback.cpp`.

## iOS pipeline today

1. User taps Start Recording.
2. App requests/checks microphone permission.
3. AVFoundation records a foreground CAF file.
4. The app decodes recorded audio to normalized PCM.
5. Placeholder transcriber returns bridge-pending copy.

Evidence: `ios/AutoWhisperApp/IOSAudioRecorder.swift`, `ios/AutoWhisperApp/IOSAudioDecoder.swift`, `ios/AutoWhisperApp/IOSWhisperTranscriber.swift`.

## Missing pipeline proof

- iOS whisper.cpp bridge.
- Device runtime proof.
- Benchmark corpus and output comparison.
- Interruption/background handling for mobile recording.

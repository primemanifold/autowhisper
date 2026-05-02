# AutoWhisper iOS Foundation

This directory is the native iOS product line for AutoWhisper.

## Current milestone

The current checked-in slice includes both:

1. A Foundation-only Swift package that verifies the mobile product contract.
2. A runnable SwiftUI iOS app shell generated from `ios/project.yml` with XcodeGen.

This is intentionally still an app-shell milestone: it proves launch, microphone-permission copy, foreground record/stop UI, placeholder local transcript flow, copy/share actions, and iOS-safe product constraints. It does not yet bind AVFoundation recording buffers to `whisper.cpp` inference.

## Verified now

From the repository root:

```bash
swift run --package-path ios AutoWhisperCoreChecks
python3 -m unittest tests.static.test_ios_app_shell -v
```

From `ios/` after generating the Xcode project:

```bash
xcodegen generate
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp \
  -destination 'generic/platform=iOS Simulator' \
  -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO build
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp \
  -destination 'generic/platform=iOS' \
  -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO build
```

The package check executable verifies:

- iOS defaults use 16 kHz mono audio for Whisper compatibility.
- The first iOS catalog recommends `tiny.en`, not the large desktop default.
- First-release models are represented as bundled resources.
- iOS output is copy/share, not desktop text injection.
- iOS-specific unavailable capabilities are explicit.
- A fake record -> transcribe -> copy/share workflow works end to end at the domain layer.

The SwiftUI shell verifies:

- AutoWhisper launches as a native iOS app target.
- The app declares `NSMicrophoneUsageDescription`.
- The app includes an App Store privacy manifest with no tracking or collected-data declarations for this shell.
- The home screen communicates iOS limits honestly: no global hotkeys and no arbitrary text injection.
- Users can exercise the foreground Start Recording -> Stop & Transcribe -> Copy Transcript / Share Transcript loop with a placeholder local transcript.

## Target first real transcription loop

The next real app milestone is:

1. Launch AutoWhisper on iPhone.
2. Request microphone permission.
3. Tap to record.
4. Tap to stop.
5. Feed recorded 16 kHz mono audio to `whisper.cpp`.
6. Display transcript in the app.
7. Copy or share the transcript.

## Deliberately not claimed on iOS

These desktop capabilities are not available in this iOS product surface:

- Global push-to-talk hotkeys.
- Background daemon mode.
- Menu-bar/system-tray app behavior.
- Injecting text into arbitrary active apps.
- Linux/X11 clipboard or `xdotool`/`xclip` behavior.
- PulseAudio mute-other-apps behavior.

A future iOS keyboard extension or Share extension may provide separate integration surfaces, but those need their own sandbox and App Store review design.

## Xcode project policy

`ios/project.yml` is the source of truth. Generate `AutoWhisperIOS.xcodeproj` locally with XcodeGen; do not hand-edit generated project files.

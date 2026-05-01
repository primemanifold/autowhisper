# AutoWhisper iOS Foundation

This directory is the start of the native iOS product line for AutoWhisper.

## Current milestone

The current checked-in slice is intentionally a Foundation-only Swift package, not yet a runnable iOS app bundle. It defines and verifies the mobile product contract we need before adding SwiftUI, AVFoundation, signing, simulator, and device builds.

Why this first:

- The current machine only has Command Line Tools selected, not full Xcode.
- `iphoneos` / `iphonesimulator` SDKs and `simctl` are not available locally yet.
- AutoWhisper's current desktop daemon model cannot be ported 1:1 to iOS because iOS does not allow global hotkeys, menu-bar daemons, or arbitrary-app text injection.

## Verified now

```bash
swift run --package-path ios AutoWhisperCoreChecks
```

The check executable verifies:

- iOS defaults use 16 kHz mono audio for Whisper compatibility.
- The first iOS catalog recommends `tiny.en`, not the large desktop default.
- First-release models are represented as bundled resources.
- iOS output is copy/share, not desktop text injection.
- iOS-specific unavailable capabilities are explicit.
- A fake record -> transcribe -> copy/share workflow works end to end at the domain layer.

## Target first app loop

The first real app milestone is:

1. Launch AutoWhisper on iPhone.
2. Request microphone permission.
3. Tap to record.
4. Tap to stop.
5. Transcribe locally with `whisper.cpp`.
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

## Full Xcode gate

Before adding and validating a runnable iOS app target, this machine needs full Xcode selected:

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
xcodebuild -version
xcrun --sdk iphoneos --show-sdk-path
xcrun --sdk iphonesimulator --show-sdk-path
xcrun --find simctl
```

Then the next gates become simulator/device `xcodebuild` builds and UI smoke tests.

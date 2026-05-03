# iOS Widget + Small Voice Plan

## Status

This plan captures the clean-room path for a small iOS voice surface inspired by Ghost Pepper's native-app approach without copying Ghost Pepper code.

## Licensing boundary

Ghost Pepper is useful as an architecture reference, but the inspected upstream repository did not expose a recognized GitHub license and no local `LICENSE*`/`COPYING*` file was found. AutoWhisper must therefore treat it as product/architecture inspiration only:

- Do not vendor Ghost Pepper source.
- Do not copy implementations, comments, UI copy, or project files.
- Use Apple's public frameworks and AutoWhisper's existing codebase to build equivalent concepts clean-room.
- If a fork is created, treat it as a research fork only unless explicit license/permission is obtained.

## Clean-room gap analysis

### Ghost Pepper patterns worth learning from

- Small native entry points that reduce friction before recording.
- Clear separation between UI, permission flows, recorder services, transcription/model management, and app state.
- Privacy-first copy that explains when audio/screen capture occurs.
- Native Settings/onboarding surfaces for sensitive permissions.

### AutoWhisper iOS before this slice

- Runnable SwiftUI foreground app shell.
- Microphone permission copy.
- Native AVFoundation foreground recorder.
- Recorded CAF/PCM decode seam.
- Placeholder transcriber seam.
- Bundled model locator seam.
- No widget/deep-link quick entry.
- No real `whisper.cpp` inference.
- No physical-device runtime proof.

### iOS platform constraints

- iOS widgets cannot directly run continuous microphone capture.
- WidgetKit should be a launcher/status surface, not a recorder process.
- Recording must remain explicit in the foreground app with microphone permission controlled by the user.
- Global hotkeys, background daemons, menu-bar behavior, and arbitrary text injection are desktop-only claims and must remain out of iOS copy.

## Implemented first slice

The first safe slice is a Quick Record widget launcher:

1. Add `AutoWhisperWidget` as a WidgetKit app-extension target in `ios/project.yml`.
2. Add a small `.systemSmall` widget that opens `autowhisper://record`.
3. Register the `autowhisper` URL scheme in the iOS app Info.plist.
4. Handle `autowhisper://record` in `ContentView`/`AutoWhisperAppModel`.
5. If microphone permission is not determined, request it in the foreground app.
6. If permission is granted and the app is idle, start the existing foreground `IOSAudioRecorder` path.
7. Keep the widget itself free of `AVAudioRecorder`, `AVAudioSession`, or microphone permission calls.
8. Document that the widget opens the app; it does not record in the widget/background process.

## Verification gates

- Static regression tests assert the widget target, URL scheme, deep-link handling, honest docs, and widget runtime boundaries.
- Swift package check still passes.
- XcodeGen regeneration succeeds.
- iOS Simulator generic build succeeds with widget embedded.
- iOS device generic build succeeds with widget embedded and `CODE_SIGNING_ALLOWED=NO`.
- `git diff --check` passes.

## Next slices

1. Add simulator install/launch/deep-link smoke evidence for `autowhisper://record`.
2. Add an iOS 17+ interactive widget/AppIntent follow-up only after preserving iOS 16 fallback behavior.
3. Add physical-device signed runtime proof for microphone permission + widget tap.
4. Only after widget/deep-link proof, move to real `whisper.cpp` linking/inference slices.

## Not claimed

- No real iOS Whisper transcript output yet.
- No background/widget microphone recording.
- No Ghost Pepper code reuse.
- No App Store/TestFlight readiness.
- No physical-device proof yet.

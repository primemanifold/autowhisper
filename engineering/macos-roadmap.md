# AutoWhisper macOS Roadmap

## Strategic decision

AutoWhisper should become a first-class macOS product while preserving the local-first dictation mission. macOS is not a side port. It is likely the platform where AutoWhisper will face the strongest direct comparison against Wispr Flow, Superwhisper, MacWhisper, Aqua Voice, and Apple Dictation.

## Current repo reality

The CMake build already has an `APPLE` branch and placeholder platform files:

- `src/hotkey/platform/hotkey_macos.mm`
- `src/output/platform/output_macos.mm`
- `src/tray/platform/tray_macos.mm`

The current placeholders warn that macOS hotkey, text injection, and tray behavior are not implemented. The repo should not market macOS as product-ready yet.

## Product bar for macOS

A real macOS version needs:

1. Native menu bar app with clear idle, recording, transcribing, and error states.
2. Global hotkey support with proper Accessibility/Input Monitoring permission flow.
3. Text insertion into active apps using macOS-safe APIs, with clipboard fallback.
4. Microphone permission flow and device selection.
5. Bundled or managed local Whisper models.
6. Signed and notarized `.app` distribution.
7. First-run onboarding that gets to first successful dictation in under two minutes.
8. Local-first proof panel that does not overclaim without diagnostics.

## Engineering phases

### Phase M0: Build truth

- Verify current CMake build on macOS.
- Document exact Homebrew/Xcode prerequisites.
- Make placeholder platform code either compile cleanly or fail with explicit unsupported messaging.
- Add local macOS build command to docs.

### Phase M1: Native platform substrate

- Implement global hotkey capture using Carbon or CGEventTap.
- Implement output injection and clipboard fallback.
- Implement NSStatusItem menu bar integration.
- Replace PulseAudio-specific assumptions with platform-neutral audio policy boundaries.

### Phase M2: Permissions and onboarding

- Add first-run permission checks for microphone, Accessibility, and Input Monitoring.
- Add actionable remediation copy.
- Add settings UI diagnostics for macOS permissions.

### Phase M3: Packaging

- Produce `.app` bundle.
- Sign and notarize.
- Define update path.
- Create release smoke tests.

### Phase M4: Competitive quality

- Build latency and accuracy benchmark harness.
- Compare against sourced competitor claims and measured local baselines.
- Tune default model and performance choices for Apple Silicon.

## Architecture note

Keep the core dictation engine platform-neutral. Platform files should own only OS boundaries: hotkeys, text insertion, tray/menu bar, permissions, and packaging. The settings UI and design system should remain shared.

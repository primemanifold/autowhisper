# Claude Scout Report — ios

Source JSON: `/tmp/autowhisper-claude-scouts/reports/ios.rerun.json`

Model/session metadata: subtype `success`, turns `11`, cost `0.1772749`.

---

---

## AutoWhisper iOS Mobile Readiness — PR #12 Scout Report

### 1. Current mobile proof levels

| Proof level | Status | Evidence |
|---|---|---|
| **Swift package build** | `ready` | `ios/Package.swift` + `ios/Sources/AutoWhisperCore/` + `ios/Sources/AutoWhisperCoreChecks/main.swift`; `swift run --package-path ios AutoWhisperCoreChecks` documented in README |
| **App shell (static tests)** | `ready` | `tests/static/test_ios_app_shell.py` — 8 test methods covering project.yml, Info.plist, privacy manifest, recorder, decoder, transcriber seam, model locator, widget, deep-link, and honest-docs checks |
| **Simulator build** | `build_proven` (command documented, not run here) | `xcodebuild ... -destination 'generic/platform=iOS Simulator' ... CODE_SIGNING_ALLOWED=NO build` per `ios/README.md:28-34` — proves XcodeGen + SDK compile; does **not** prove launch, microphone permission runtime, or AVFoundation recording |
| **Generic device build** | `build_proven` (command documented, not run here) | `xcodebuild ... -destination 'generic/platform=iOS' -sdk iphoneos CODE_SIGNING_ALLOWED=NO build` — proves arm64 compilation; does **not** prove install, sandbox, or microphone grant |
| **Physical-device runtime** | `unverified` | No signed IPA, no Apple Developer account provisioning, no recorded device session anywhere in the tree |
| **Widget / deep-link proof** | `partial` | Widget target exists, URL scheme registered, `.onOpenURL` handler in `ContentView.swift`; static tests assert these paths but no simulator/device tap evidence |
| **Real local transcription** | `planned` | `IOSPlaceholderWhisperTranscriber` (line 8 of `IOSWhisperTranscriber.swift`) returns a text stub; `whisper_init`/`whisper_full` are explicitly absent (asserted by `test_ios_model_resources_have_locator_before_whisper_inference`); no `deps/whisper.cpp` link in `Package.swift` |
| **TestFlight / App Store** | `planned` | No provisioning profile, no entitlements file, `CODE_SIGNING_ALLOWED: NO` in `project.yml:42,52` |

---

### 2. Top 5 next mobile gaps

1. **No `whisper.cpp` C-API link.** `ios/Package.swift` has no binary/system target for whisper.cpp. `IOSPlaceholderWhisperTranscriber` only echoes decode stats. Gap between `IOSAudioDecoder` output and real inference is fully open.

2. **Physical-device proof is entirely absent.** No developer team ID, no provisioning profile, no entitlements, no record of a device build being installed. `CODE_SIGNING_ALLOWED: NO` blocks on-device install categorically.

3. **Simulator launch/screenshot evidence does not exist.** The two `xcodebuild` commands in README are documented claims, not recorded evidence. No CI job, no screenshot, no `xcresult` artifact is referenced anywhere.

4. **Deep-link / widget runtime smoke is static only.** The `test_ios_widget_and_deep_link_are_safe_quick_record_surfaces` test reads source text; it does not open the app via `autowhisper://record` in a running simulator, so the `.onOpenURL` branch has never been exercised at runtime.

5. **`ModelStorage` only has `.bundledResource`; actual model binary is absent.** `ModelCatalog.swift:63` defines only `bundledResource`. The `ios/AutoWhisperApp/Models/` directory is referenced in `project.yml:24` as a resource folder but no `.bin` files are committed. The model locator will always hit `missingBundledModel` on a real device.

---

### 3. Smallest safe iOS slice after current widget launcher

**Slice: Simulator install + deep-link smoke evidence.**

Specifically:
- Boot a simulator (`xcrun simctl boot`).
- Install the `.app` bundle (`xcrun simctl install`).
- Open `autowhisper://record` (`xcrun simctl openurl`).
- Capture a screenshot (`xcrun simctl io screenshot`).

This proves `.onOpenURL` fires and the permission request is reached — without requiring a Developer account, signing, or a real device. It fills the biggest unbridged gap between static-text tests and runtime proof and is a prerequisite for every subsequent slice (whisper bridge, physical device, TestFlight).

---

### 4. Commands for simulator / generic device proof — and what they do not prove

#### Simulator build
```bash
cd ios
xcodegen generate
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp \
  -destination 'generic/platform=iOS Simulator' \
  -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO build
```
**Proves:** XcodeGen, Swift 6 compilation, dependency resolution, widget target compiles.
**Does not prove:** app launches, microphone permission request fires, AVFoundation session opens, `onOpenURL` is reached, widget tap opens the app, or any inference path executes.

#### Generic device build
```bash
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp \
  -destination 'generic/platform=iOS' \
  -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO build
```
**Proves:** arm64 compilation succeeds, no iOS-specific SDK symbol errors.
**Does not prove:** the IPA can be installed (`CODE_SIGNING_ALLOWED=NO` prevents install), sandbox permissions work, microphone is granted on real hardware, or the widget can tap through on a real home screen.

#### Swift package check
```bash
swift run --package-path ios AutoWhisperCoreChecks
```
**Proves:** `AutoWhisperCore` domain logic compiles and all package-level checks pass.
**Does not prove:** anything about the Xcode targets, AVFoundation runtime, or app lifecycle.

---

### 5. Claim-boundary tests — keeping widget and transcription language honest

The existing `test_ios_app_shell.py` already enforces the key boundaries:

| Test | What it guards | Location |
|---|---|---|
| `test_ios_shell_decodes_recorded_audio_for_whisper_bridge` | `assertNotIn("Stop & Transcribe", view_text)`, `assertNotIn("transcribe locally", view_text)`, `assertNotIn("recordingState = .transcribed", view_text)` — prevents the UI claiming real transcription | `test_ios_app_shell.py:153-158` |
| `test_ios_model_resources_have_locator_before_whisper_inference` | `assertNotIn("whisper_init", ...)`, `assertNotIn("whisper_full", ...)` in both locator and transcriber — blocks premature whisper.cpp binding claims | `test_ios_app_shell.py:190-203` |
| `test_ios_widget_and_deep_link_are_safe_quick_record_surfaces` | `assertNotIn` for `AVAudioRecorder`, `AVAudioSession`, `requestAccess(for: .audio)`, `startRecording()` in widget source — blocks widget from claiming audio recording | `test_ios_app_shell.py:261-263` |
| `test_ios_widget_voice_docs_are_honest_about_platform_limits` | README must contain `"Widgets cannot record microphone audio directly"` and `"opens the foreground app"` | `test_ios_app_shell.py:265-275` |

**Two gaps in current claim-boundary tests:**

1. No test asserts that `ios/README.md` does **not** contain "local transcription" or "transcribes on device" — the README test only checks for presence of honest strings, not absence of overclaims.
2. No test asserts the `IOSWhisperTranscriber` return value contains `"Whisper bridge pending"` (it does today at `IOSWhisperTranscriber.swift:30`), which would catch a future replacement that silently promoted to a claimed real transcript. Worth adding as a static regression.

# AutoWhisper iOS Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Build AutoWhisper for iOS as a native, local-first recorder/transcriber app without overclaiming desktop-only automation.

**Architecture:** iOS is a sibling product surface. It should reuse `whisper.cpp` and platform-neutral product concepts, but not the Linux/macOS daemon, global hotkey, tray, or arbitrary text-injection flow. The first implementation layer is a Foundation-only Swift package that can be locally verified before full Xcode is available.

**Tech Stack:** Swift Package Manager, Swift 6, Foundation, future SwiftUI + AVFoundation + whisper.cpp C API, future Xcode/iOS SDK.

---

## Phase I0: Foundation contract

### Task 1: Add Swift package skeleton

**Objective:** Create `ios/` as a first-party Swift package that builds with the available Swift CLI.

**Files:**
- Create: `ios/Package.swift`
- Create: `ios/Sources/AutoWhisperCore/`
- Create: `ios/Sources/AutoWhisperCoreChecks/main.swift`

**Verification:**

```bash
swift run --package-path ios AutoWhisperCoreChecks
```

Expected before implementation: compile failures for missing domain types.
Expected after implementation: `AutoWhisperCoreChecks passed`.

### Task 2: Define mobile product defaults

**Objective:** Encode iOS-safe defaults: small bundled model, 16 kHz mono recording, copy/share output, local-only privacy, unavailable desktop capabilities.

**Files:**
- Create: `ios/Sources/AutoWhisperCore/AppSettings.swift`
- Create: `ios/Sources/AutoWhisperCore/ModelCatalog.swift`

**Verification:**

```bash
swift run --package-path ios AutoWhisperCoreChecks
```

### Task 3: Define iOS permission/capability model

**Objective:** Make microphone blocking and iOS platform limitations explicit in code and user-facing copy.

**Files:**
- Create: `ios/Sources/AutoWhisperCore/Permissions.swift`

**Verification:**

```bash
swift run --package-path ios AutoWhisperCoreChecks
```

### Task 4: Define record/transcribe workflow seam

**Objective:** Provide a testable domain seam for record -> transcribe -> copy/share before binding to AVFoundation or whisper.cpp.

**Files:**
- Create: `ios/Sources/AutoWhisperCore/Transcription.swift`

**Verification:**

```bash
swift run --package-path ios AutoWhisperCoreChecks
```

## Phase I1: Xcode app shell

Status: implemented as the first runnable SwiftUI app-shell slice.

**Objective:** Add a runnable SwiftUI iOS app target.

**Files:**
- Created: `ios/project.yml` as the XcodeGen source of truth.
- Created: `ios/AutoWhisperApp/AutoWhisperApp.swift`
- Created: `ios/AutoWhisperApp/ContentView.swift`
- Created: `ios/AutoWhisperApp/Info.plist`
- Created: `ios/AutoWhisperApp/PrivacyInfo.xcprivacy`
- Created: `ios/AutoWhisperApp/LaunchScreen.storyboard`

**Required Info.plist key:**

```xml
<key>NSMicrophoneUsageDescription</key>
<string>AutoWhisper records your voice only when you tap record so it can transcribe locally on this device.</string>
```

**Verification:**

```bash
swift run --package-path ios AutoWhisperCoreChecks
python3 -m unittest tests.static.test_ios_app_shell -v
cd ios && xcodegen generate
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp \
  -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO build
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp \
  -destination 'generic/platform=iOS' -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO build
```

Runtime smoke evidence is simulator install + launch + screenshot.

## Phase I2: Native audio and whisper bridge

Status: native AVFoundation recording is implemented; `whisper.cpp` inference bridge remains next.

**Objective:** Bind the app shell to AVFoundation recording and `whisper.cpp` local inference.

**Approach:**
- Implemented: use AVFoundation for microphone permission-gated foreground recording.
- Implemented: record 16 kHz mono Linear PCM CAF files through `IOSAudioRecorder`.
- Next: decode/stream that recorded audio into bundled `deps/whisper.cpp` C API as the first inference bridge.
- Keep model loading serialized through an actor or equivalent concurrency boundary.

## Phase I3: Distribution-grade iOS behavior

**Objective:** Move from simulator proof to TestFlight-ready quality.

**Required gates:**
- Device build with signing/provisioning.
- Microphone permission denied/granted flows.
- Missing model error flow.
- Local-only proof copy and diagnostics.
- Copy/share smoke tests.
- App Store privacy posture review.

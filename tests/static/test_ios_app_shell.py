import plistlib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CI_WORKFLOW = ROOT / ".github" / "workflows" / "ci.yml"
IOS = ROOT / "ios"
PROJECT_YML = IOS / "project.yml"
APP_DIR = IOS / "AutoWhisperApp"
APP_SWIFT = APP_DIR / "AutoWhisperApp.swift"
CONTENT_VIEW = APP_DIR / "ContentView.swift"
RECORDER = APP_DIR / "IOSAudioRecorder.swift"
DECODER = APP_DIR / "IOSAudioDecoder.swift"
TRANSCRIBER = APP_DIR / "IOSWhisperTranscriber.swift"
MODEL_LOCATOR = APP_DIR / "IOSWhisperModelLocator.swift"
MODEL_CATALOG = IOS / "Sources" / "AutoWhisperCore" / "ModelCatalog.swift"
INFO_PLIST = APP_DIR / "Info.plist"
PRIVACY = APP_DIR / "PrivacyInfo.xcprivacy"
README = IOS / "README.md"
WIDGET_DIR = IOS / "AutoWhisperWidget"
WIDGET_SWIFT = WIDGET_DIR / "AutoWhisperWidget.swift"
WIDGET_INTENTS = WIDGET_DIR / "AutoWhisperWidgetIntents.swift"


class IOSAppShellTests(unittest.TestCase):
    def test_xcodegen_project_declares_runnable_ios_app(self):
        self.assertTrue(PROJECT_YML.exists(), "ios/project.yml should be the source of truth for the generated Xcode project")
        text = PROJECT_YML.read_text()
        for snippet in [
            "name: AutoWhisperIOS",
            "type: application",
            "platform: iOS",
            "AutoWhisperApp",
            "AutoWhisperCore",
            "INFOPLIST_FILE: AutoWhisperApp/Info.plist",
            "PRODUCT_BUNDLE_IDENTIFIER: com.primemanifold.autowhisper.ios",
            "CODE_SIGN_STYLE: Automatic",
        ]:
            self.assertIn(snippet, text)

    def test_ios_app_has_microphone_and_privacy_metadata(self):
        self.assertTrue(INFO_PLIST.exists(), "iOS app shell must declare an Info.plist")
        info = plistlib.loads(INFO_PLIST.read_bytes())
        self.assertEqual(info["CFBundleDisplayName"], "AutoWhisper")
        self.assertEqual(info["CFBundleIdentifier"], "$(PRODUCT_BUNDLE_IDENTIFIER)")
        mic_usage = info["NSMicrophoneUsageDescription"]
        self.assertIn("records your voice only when you tap record", mic_usage)
        self.assertIn("decode audio locally for the transcription bridge", mic_usage)
        self.assertNotIn("transcribe locally on this device", mic_usage)
        self.assertEqual(info["UILaunchStoryboardName"], "LaunchScreen")
        self.assertIn("UIInterfaceOrientationPortrait", info["UISupportedInterfaceOrientations"])

        self.assertTrue(PRIVACY.exists(), "iOS app shell must include an App Store privacy manifest")
        privacy = plistlib.loads(PRIVACY.read_bytes())
        self.assertEqual(privacy.get("NSPrivacyTracking"), False)
        self.assertEqual(privacy.get("NSPrivacyCollectedDataTypes"), [])
        self.assertEqual(privacy.get("NSPrivacyAccessedAPITypes"), [])

    def test_swiftui_shell_uses_mobile_contract_and_does_not_claim_desktop_features(self):
        self.assertTrue(APP_SWIFT.exists())
        self.assertTrue(CONTENT_VIEW.exists())
        app_text = APP_SWIFT.read_text()
        view_text = CONTENT_VIEW.read_text()
        combined = app_text + "\n" + view_text
        for snippet in [
            "import SwiftUI",
            "import AutoWhisperCore",
            "@main",
            "AutoWhisperIOSApp",
            "AutoWhisperSettings.mobileDefaults",
            "IOSPlaceholderWhisperTranscriber",
            "Copy Transcript",
            "Share Transcript",
            "iOS does not allow global hotkeys",
        ]:
            self.assertIn(snippet, combined)
        forbidden = ["global hotkey is available", "inject into any app", "menu bar daemon"]
        for snippet in forbidden:
            self.assertNotIn(snippet, combined.lower())

    def test_ios_shell_uses_native_avfoundation_recorder_before_transcription_bridge(self):
        self.assertTrue(RECORDER.exists(), "iOS shell should use a native AVFoundation recorder service, not only synthetic fixtures")
        recorder_text = RECORDER.read_text()
        view_text = CONTENT_VIEW.read_text()
        for snippet in [
            "import AVFoundation",
            "final class IOSAudioRecorder",
            "AVAudioRecorder",
            "AVAudioSession.sharedInstance()",
            "setCategory(.playAndRecord",
            "sampleRate: Double = 16_000",
            "numberOfChannels: Int = 1",
            "linearPCMBitDepth: Int = 16",
            "recordingURL",
            "durationSeconds",
            "guard newRecorder.record() else",
            "setActive(false, options: [.notifyOthersOnDeactivation])",
            "throw IOSAudioRecorderError.failedToStart",
        ]:
            self.assertIn(snippet, recorder_text)
        for snippet in [
            "private let audioRecorder = IOSAudioRecorder()",
            "try audioRecorder.startRecording()",
            "let recording = try audioRecorder.stopRecording()",
        ]:
            self.assertIn(snippet, view_text)
        self.assertIn(".allowBluetooth", recorder_text)
        self.assertNotIn(".allowBluetoothHFP", recorder_text)
        self.assertNotIn("AudioFixture.fixture(samples: [0, 0.1, 0.2, 0.1, 0])", view_text)

    def test_ios_shell_decodes_recorded_audio_for_whisper_bridge(self):
        self.assertTrue(DECODER.exists(), "Recorded CAF audio should be decoded into normalized PCM before inference")
        self.assertTrue(TRANSCRIBER.exists(), "The app should have an explicit iOS Whisper transcriber seam")
        decoder_text = DECODER.read_text()
        transcriber_text = TRANSCRIBER.read_text()
        view_text = CONTENT_VIEW.read_text()

        for snippet in [
            "import AVFoundation",
            "struct IOSDecodedAudio",
            "final class IOSAudioDecoder",
            "AVAudioFile(forReading:",
            "AVAudioPCMBuffer",
            "read(into:",
            "floatChannelData",
            "maxFrameCount",
            "sampleRate",
            "samples: [Float]",
        ]:
            self.assertIn(snippet, decoder_text)

        for snippet in [
            "protocol IOSWhisperTranscribing",
            "final class IOSPlaceholderWhisperTranscriber",
            "IOSAudioDecoder",
            "func transcribe(recording: IOSAudioRecording) async throws -> String",
            "Task.detached(priority: .userInitiated)",
            "decoded.samples.count",
            "Whisper bridge pending",
        ]:
            self.assertIn(snippet, transcriber_text)

        for snippet in [
            "private let transcriber: IOSWhisperTranscribing",
            "IOSPlaceholderWhisperTranscriber()",
            "guard recordingState != .preparingTranscript else { return }",
            "recordingState = .preparingTranscript",
            "transcriptText = try await transcriber.transcribe(recording: recording)",
            "private var recordingButtonTitle: String",
            "case .preparingTranscript:",
            "return \"Decoding Audio…\"",
            ".disabled(model.recordingState == .preparingRecording || model.recordingState == .preparingTranscript)",
        ]:
            self.assertIn(snippet, view_text)
        self.assertNotIn("stopAndTranscribe(audio: AudioFixture(samples: []", view_text)
        for overclaim in [
            "transcribe locally",
            "Stop & Transcribe",
            "recordingState = .transcribed",
        ]:
            self.assertNotIn(overclaim, view_text)

    def test_ios_model_resources_have_locator_before_whisper_inference(self):
        self.assertTrue(MODEL_CATALOG.exists(), "iOS model catalog should declare concrete GGML resource filenames")
        catalog_text = MODEL_CATALOG.read_text()
        for snippet in [
            "ggmlFilename",
            "bundleResourceName",
            "\"ggml-tiny.en.bin\"",
            "\"ggml-base.en.bin\"",
        ]:
            self.assertIn(snippet, catalog_text)

        project_text = PROJECT_YML.read_text()
        for snippet in [
            "AutoWhisperApp/Models",
            "ModelResources.plist",
        ]:
            self.assertIn(snippet, project_text)

        self.assertTrue(MODEL_LOCATOR.exists(), "iOS app should have a model resource locator seam before real inference")
        locator_text = MODEL_LOCATOR.read_text()
        for snippet in [
            "final class IOSWhisperModelLocator",
            "Bundle",
            "url(forResource:",
            "IOSWhisperModelLocatorError",
            "case missingBundledModel",
            "ggmlFilename",
            "subdirectory: \"Models\"",
        ]:
            self.assertIn(snippet, locator_text)
        self.assertNotIn("whisper_init", locator_text)
        self.assertNotIn("whisper_full", locator_text)

        transcriber_text = TRANSCRIBER.read_text()
        for snippet in [
            "private let modelLocator",
            "IOSWhisperModelLocator",
            "try modelLocator.url(for: ModelCatalog.mobileDefault.recommended)",
            "missingBundledModel",
            "Model not bundled yet",
        ]:
            self.assertIn(snippet, transcriber_text)
        self.assertNotIn("whisper_init", transcriber_text)
        self.assertNotIn("whisper_full", transcriber_text)

    def test_ios_primary_record_button_requests_microphone_permission_first(self):
        view_text = CONTENT_VIEW.read_text()
        self.assertIn("@discardableResult", view_text)
        self.assertIn("func requestMicrophonePermission() async -> Bool", view_text)
        self.assertIn("func hasMicrophonePermissionForRecording() async -> Bool", view_text)
        self.assertIn("await requestMicrophonePermission()", view_text)
        self.assertIn("guard await hasMicrophonePermissionForRecording() else {", view_text)
        toggle_body = view_text.split("func toggleRecording() async", 1)[1]
        self.assertLess(
            toggle_body.index("guard await hasMicrophonePermissionForRecording() else {"),
            toggle_body.index("try audioRecorder.startRecording()"),
            "The primary Start Recording path must request/verify microphone permission before starting AVAudioRecorder",
        )
        self.assertIn("Microphone permission is required before recording", view_text)
        self.assertNotIn("Start Recording requests permission and transcribes locally", view_text)

    def test_ios_primary_record_button_serializes_start_transitions(self):
        view_text = CONTENT_VIEW.read_text()
        core_text = (IOS / "Sources" / "AutoWhisperCore" / "Transcription.swift").read_text()
        self.assertIn("case preparingRecording", core_text)
        self.assertIn("case .preparingRecording:", view_text)
        self.assertIn("guard !isRecordingTransitionInFlight else { return }", view_text)
        self.assertIn("isRecordingTransitionInFlight = true", view_text)
        self.assertIn("defer { isRecordingTransitionInFlight = false }", view_text)
        toggle_body = view_text.split("func toggleRecording() async", 1)[1]
        self.assertLess(
            toggle_body.index("isRecordingTransitionInFlight = true"),
            toggle_body.index("guard await hasMicrophonePermissionForRecording() else {"),
            "Start Recording must mark an in-flight transition before awaiting microphone permission",
        )
        self.assertIn(".disabled(model.recordingState == .preparingRecording", view_text)

    def test_ios_hosted_ci_builds_app_and_widget(self):
        workflow_text = CI_WORKFLOW.read_text()
        for snippet in [
            "runs-on: macos-",
            "swift run --package-path ios AutoWhisperCoreChecks",
            "xcodegen generate",
            "xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp",
            "generic/platform=iOS Simulator",
            "generic/platform=iOS",
            "test -f AutoWhisperIOS.xcodeproj/project.pbxproj",
        ]:
            self.assertIn(snippet, workflow_text)

    def test_readme_reflects_runnable_app_shell_gate(self):
        text = README.read_text()
        self.assertIn("runnable SwiftUI iOS app shell", text)
        self.assertIn("xcodegen generate", text)
        self.assertIn("xcodebuild -project AutoWhisperIOS.xcodeproj", text)
        self.assertNotIn("not yet a runnable iOS app bundle", text)
        self.assertNotIn("Blocked until full Xcode/iOS SDK", text)
    def test_ios_widget_and_deep_link_are_safe_quick_record_surfaces(self):
        project_text = PROJECT_YML.read_text()
        info = plistlib.loads(INFO_PLIST.read_bytes())
        view_text = CONTENT_VIEW.read_text()

        for snippet in [
            "AutoWhisperWidget:",
            "type: app-extension",
            "platform: iOS",
            "AutoWhisperWidget",
            "PRODUCT_BUNDLE_IDENTIFIER: com.primemanifold.autowhisper.ios.widget",
            "CODE_SIGNING_ALLOWED: NO",
        ]:
            self.assertIn(snippet, project_text)

        schemes = [
            scheme
            for item in info.get("CFBundleURLTypes", [])
            for scheme in item.get("CFBundleURLSchemes", [])
        ]
        self.assertIn("autowhisper", schemes)

        for snippet in [
            ".onOpenURL",
            "handleDeepLink",
            "url.scheme == \"autowhisper\"",
            "url.host == \"record\"",
            "requestMicrophonePermission()",
        ]:
            self.assertIn(snippet, view_text)

        self.assertTrue(WIDGET_SWIFT.exists(), "Widget target should expose a small quick-record launcher")
        self.assertTrue(WIDGET_INTENTS.exists(), "Widget target should include an AppIntent/deep-link seam for future Shortcuts")
        widget_text = WIDGET_SWIFT.read_text()
        intents_text = WIDGET_INTENTS.read_text()
        combined_widget = widget_text + "\n" + intents_text
        for snippet in [
            "import WidgetKit",
            "import SwiftUI",
            "struct AutoWhisperWidget",
            "StaticConfiguration",
            "supportedFamilies([.systemSmall])",
            "autowhisper://record",
            "Open AutoWhisper to record",
            "AppIntent",
            "OpenRecorderIntent",
        ]:
            self.assertIn(snippet, combined_widget)

        forbidden_widget_runtime = ["AVAudioRecorder", "AVAudioSession", "requestAccess(for: .audio)", "startRecording()"]
        for snippet in forbidden_widget_runtime:
            self.assertNotIn(snippet, combined_widget)

    def test_ios_widget_voice_docs_are_honest_about_platform_limits(self):
        text = README.read_text()
        for snippet in [
            "Quick Record widget",
            "Widgets cannot record microphone audio directly",
            "opens the foreground app",
            "autowhisper://record",
            "No Ghost Pepper code is vendored or copied",
        ]:
            self.assertIn(snippet, text)


    # ── Interruption / background reconciliation (RED) ──────────────────────

    def test_recorder_exposes_interruption_callback_interface(self):
        """IOSAudioRecorder must expose a way for callers to observe when the OS
        stops or interrupts recording, so ContentView can reconcile state."""
        rec = RECORDER.read_text()
        # Delegate callback that fires when AVAudioRecorder stops unexpectedly
        self.assertIn("audioRecorderDidFinishRecording", rec)
        # Interruption observation: either AVAudioSession notification or a
        # typed onInterrupted callback that callers can set
        has_notification = "AVAudioSession.interruptionNotification" in rec
        has_callback = "onInterrupted" in rec
        self.assertTrue(
            has_notification or has_callback,
            "IOSAudioRecorder must observe AVAudioSession interruptions or expose onInterrupted",
        )

    def test_app_model_reconciles_state_on_recorder_interruption(self):
        """AutoWhisperAppModel must handle recorder interruption and set a
        non-recording state so the UI never shows 'recording' after the OS stops it."""
        view = CONTENT_VIEW.read_text()
        # Model must connect the recorder's interruption signal
        self.assertIn("onInterrupted", view)
        # Must transition to a well-defined non-recording state
        interrupted_handled = (
            "recordingState = .failed" in view or
            "recordingState = .idle" in view
        )
        self.assertTrue(interrupted_handled,
            "Model must set recordingState to .idle or .failed when interrupted")
        # Must surface a human-readable message so the user understands what happened
        self.assertIn("interrupted", view.lower())

    def test_app_model_refreshes_permission_on_scene_active(self):
        """AutoWhisperAppModel must refresh microphone permission status when the
        app returns to the foreground (scenePhase == .active), so that stale
        permission copy cannot persist after a Settings change."""
        view = CONTENT_VIEW.read_text()
        self.assertIn("scenePhase", view)
        self.assertIn(".active", view)
        self.assertIn("updateMicrophonePermissionStatus", view)

    def test_readme_documents_interruption_behaviour(self):
        """ios/README.md must document that recording stops when the app is
        interrupted or backgrounded, so users and QA know what to expect."""
        text = README.read_text()
        self.assertTrue(
            "interrupt" in text.lower() or "background" in text.lower(),
            "iOS README must mention interruption or background handling"
        )


if __name__ == "__main__":
    unittest.main()

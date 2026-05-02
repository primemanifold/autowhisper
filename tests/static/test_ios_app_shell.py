import plistlib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IOS = ROOT / "ios"
PROJECT_YML = IOS / "project.yml"
APP_DIR = IOS / "AutoWhisperApp"
APP_SWIFT = APP_DIR / "AutoWhisperApp.swift"
CONTENT_VIEW = APP_DIR / "ContentView.swift"
RECORDER = APP_DIR / "IOSAudioRecorder.swift"
DECODER = APP_DIR / "IOSAudioDecoder.swift"
TRANSCRIBER = APP_DIR / "IOSWhisperTranscriber.swift"
INFO_PLIST = APP_DIR / "Info.plist"
PRIVACY = APP_DIR / "PrivacyInfo.xcprivacy"
README = IOS / "README.md"


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
            ".disabled(model.recordingState == .preparingTranscript)",
        ]:
            self.assertIn(snippet, view_text)
        self.assertNotIn("stopAndTranscribe(audio: AudioFixture(samples: []", view_text)
        for overclaim in [
            "transcribe locally",
            "Stop & Transcribe",
            "recordingState = .transcribed",
        ]:
            self.assertNotIn(overclaim, view_text)

    def test_readme_reflects_runnable_app_shell_gate(self):
        text = README.read_text()
        self.assertIn("runnable SwiftUI iOS app shell", text)
        self.assertIn("xcodegen generate", text)
        self.assertIn("xcodebuild -project AutoWhisperIOS.xcodeproj", text)
        self.assertNotIn("not yet a runnable iOS app bundle", text)
        self.assertNotIn("Blocked until full Xcode/iOS SDK", text)


if __name__ == "__main__":
    unittest.main()

import AutoWhisperCore

@inline(__always)
func check(_ condition: @autoclosure () -> Bool, _ message: String) {
    if !condition() {
        fatalError("Check failed: \(message)")
    }
}

func checkDefaultMobileSettingsAreIOSAppropriate() {
    let settings = AutoWhisperSettings.mobileDefaults

    check(settings.defaultModel.id == "tiny.en", "default iOS model should be tiny.en")
    check(settings.recording.sampleRate == 16_000, "iOS recorder should target whisper-compatible 16 kHz audio")
    check(settings.recording.channelCount == 1, "iOS recorder should use mono audio")
    check(settings.outputMode == .copyAndShare, "iOS output should be copy/share, not desktop text injection")
    check(settings.privacy.localTranscriptionOnly, "iOS defaults should preserve local-only transcription")
    check(!settings.features.globalHotkeysAvailable, "iOS should not expose global hotkeys")
    check(!settings.features.arbitraryTextInsertionAvailable, "iOS should not claim arbitrary text injection")
}

func checkModelCatalogPrefersSmallBundledModelsForFirstIOSRelease() {
    let catalog = ModelCatalog.mobileDefault

    check(catalog.recommended.id == "tiny.en", "recommended iOS model should be tiny.en")
    check(catalog.recommended.approximateSizeMB <= 100, "first iOS model should be mobile-sized")
    check(catalog.supported.allSatisfy { $0.storage == .bundledResource }, "first iOS models should be bundled resources")
    check(!catalog.supported.contains { $0.id == "distil-large-v3" }, "large desktop default should not be in first iOS catalog")
}

func checkPermissionGateExplainsRequiredAndUnavailableCapabilities() {
    let gate = PermissionGate(
        microphone: .denied,
        speechRecognition: .notRequired,
        globalHotkeys: .unavailableOnIOS,
        arbitraryTextInsertion: .unavailableOnIOS
    )

    check(!gate.canRecord, "denied microphone should block recording")
    check(gate.blockingMessages == ["Microphone access is required to record dictation."], "mic denial should have actionable blocking copy")
    check(gate.unavailableCapabilityMessages.contains("iOS does not allow global hotkeys for third-party apps."), "global hotkey limitation should be explicit")
    check(gate.unavailableCapabilityMessages.contains("iOS does not allow third-party apps to inject text into arbitrary apps."), "text injection limitation should be explicit")
}

func checkRecorderTranscriberWorkflowProducesCopyableTranscript() async throws {
    let engine = FakeTranscriptionEngine(result: "hello from ios")
    let session = RecordingSession(engine: engine)

    let initialState = await session.state
    check(initialState == .idle, "session should start idle")
    try await session.startRecording()
    let recordingState = await session.state
    check(recordingState == .recording, "session should enter recording state")

    let transcript = try await session.stopAndTranscribe(audio: .fixture(samples: [0.1, 0.2, 0.1]))

    check(transcript.text == "hello from ios", "transcript should come from engine")
    check(transcript.recommendedActions == [.copyToClipboard, .share], "iOS transcript should be copy/share ready")
    let finalState = await session.state
    check(finalState == .transcribed, "session should finish transcribed")
}

func checkTranscriptionFailureDoesNotLeaveSessionStuck() async throws {
    let session = RecordingSession(engine: FailingTranscriptionEngine())

    try await session.startRecording()
    do {
        _ = try await session.stopAndTranscribe(audio: .fixture(samples: [0.1]))
        fatalError("expected transcription failure")
    } catch {
        let failedState = await session.state
        check(failedState == .failed, "session should enter failed state after transcription error")
    }
}

@main
struct AutoWhisperCoreChecks {
    static func main() async throws {
        checkDefaultMobileSettingsAreIOSAppropriate()
        checkModelCatalogPrefersSmallBundledModelsForFirstIOSRelease()
        checkPermissionGateExplainsRequiredAndUnavailableCapabilities()
        try await checkRecorderTranscriberWorkflowProducesCopyableTranscript()
        try await checkTranscriptionFailureDoesNotLeaveSessionStuck()
        print("AutoWhisperCoreChecks passed")
    }
}

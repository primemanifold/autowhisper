import Foundation
import AutoWhisperCore

protocol IOSWhisperTranscribing: Sendable {
    func transcribe(recording: IOSAudioRecording) async throws -> String
}

final class IOSPlaceholderWhisperTranscriber: IOSWhisperTranscribing {
    private let modelLocator: IOSWhisperModelLocator

    init(modelLocator: IOSWhisperModelLocator = IOSWhisperModelLocator()) {
        self.modelLocator = modelLocator
    }

    func transcribe(recording: IOSAudioRecording) async throws -> String {
        let decoded = try await Task.detached(priority: .userInitiated) {
            try IOSAudioDecoder().decode(url: recording.recordingURL)
        }.value
        let seconds = recording.durationSeconds.formatted(.number.precision(.fractionLength(1)))
        let sampleRate = Int(decoded.sampleRate.rounded())
        let modelStatus: String
        do {
            let modelURL = try modelLocator.url(for: ModelCatalog.mobileDefault.recommended)
            modelStatus = "Model resource ready: \(modelURL.lastPathComponent)."
        } catch IOSWhisperModelLocatorError.missingBundledModel(let filename) {
            modelStatus = "Model not bundled yet: \(filename)."
        }

        return """
        Whisper bridge pending — native iOS audio decode succeeded.

        Recorded \(seconds)s of foreground iOS audio.
        Decoded \(decoded.samples.count) mono PCM samples at \(sampleRate) Hz from \(decoded.sourceURL.lastPathComponent).
        \(modelStatus)
        Next slice: feed these samples into bundled whisper.cpp and replace this placeholder with local model output.
        """
    }
}

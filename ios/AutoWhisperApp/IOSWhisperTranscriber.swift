import Foundation

protocol IOSWhisperTranscribing: Sendable {
    func transcribe(recording: IOSAudioRecording) async throws -> String
}

final class IOSPlaceholderWhisperTranscriber: IOSWhisperTranscribing {
    func transcribe(recording: IOSAudioRecording) async throws -> String {
        let decoded = try await Task.detached(priority: .userInitiated) {
            try IOSAudioDecoder().decode(url: recording.recordingURL)
        }.value
        let seconds = recording.durationSeconds.formatted(.number.precision(.fractionLength(1)))
        let sampleRate = Int(decoded.sampleRate.rounded())

        return """
        Whisper bridge pending — native iOS audio decode succeeded.

        Recorded \(seconds)s of foreground iOS audio.
        Decoded \(decoded.samples.count) mono PCM samples at \(sampleRate) Hz from \(decoded.sourceURL.lastPathComponent).
        Next slice: feed these samples into bundled whisper.cpp and replace this placeholder with local model output.
        """
    }
}

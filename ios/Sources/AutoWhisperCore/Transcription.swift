import Foundation

public struct AudioFixture: Equatable, Sendable {
    public var samples: [Float]
    public var sampleRate: Int

    public init(samples: [Float], sampleRate: Int = 16_000) {
        self.samples = samples
        self.sampleRate = sampleRate
    }

    public static func fixture(samples: [Float], sampleRate: Int = 16_000) -> AudioFixture {
        AudioFixture(samples: samples, sampleRate: sampleRate)
    }
}

public protocol TranscriptionEngine: Sendable {
    func transcribe(audio: AudioFixture) async throws -> Transcript
}

public struct Transcript: Equatable, Sendable {
    public var text: String
    public var recommendedActions: [TranscriptAction]

    public init(text: String, recommendedActions: [TranscriptAction] = [.copyToClipboard, .share]) {
        self.text = text
        self.recommendedActions = recommendedActions
    }
}

public enum TranscriptAction: Equatable, Sendable {
    case copyToClipboard
    case share
}

public enum RecordingState: Equatable, Sendable {
    case idle
    case recording
    case transcribing
    case transcribed
    case failed
}

public enum RecordingSessionError: Error, Equatable {
    case alreadyRecording
    case notRecording
    case transcriptionFailed
}

public actor RecordingSession {
    private let engine: any TranscriptionEngine
    public private(set) var state: RecordingState = .idle

    public init(engine: any TranscriptionEngine) {
        self.engine = engine
    }

    public func startRecording() async throws {
        guard state != .recording else { throw RecordingSessionError.alreadyRecording }
        state = .recording
    }

    public func stopAndTranscribe(audio: AudioFixture) async throws -> Transcript {
        guard state == .recording else { throw RecordingSessionError.notRecording }
        state = .transcribing
        do {
            let transcript = try await engine.transcribe(audio: audio)
            state = .transcribed
            return transcript
        } catch {
            state = .failed
            throw error
        }
    }
}

public struct FakeTranscriptionEngine: TranscriptionEngine {
    public var result: String

    public init(result: String) {
        self.result = result
    }

    public func transcribe(audio: AudioFixture) async throws -> Transcript {
        Transcript(text: result)
    }
}

public struct FailingTranscriptionEngine: TranscriptionEngine {
    public init() {}

    public func transcribe(audio: AudioFixture) async throws -> Transcript {
        throw RecordingSessionError.transcriptionFailed
    }
}

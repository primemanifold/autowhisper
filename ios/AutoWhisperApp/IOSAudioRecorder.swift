import AVFoundation
import Foundation

struct IOSAudioRecording: Equatable, Sendable {
    let recordingURL: URL
    let durationSeconds: TimeInterval
    let sampleRate: Double
    let numberOfChannels: Int
}

struct IOSAudioRecorderConfiguration: Equatable {
    var sampleRate: Double
    var numberOfChannels: Int
    var linearPCMBitDepth: Int

    init(sampleRate: Double = 16_000, numberOfChannels: Int = 1, linearPCMBitDepth: Int = 16) {
        self.sampleRate = sampleRate
        self.numberOfChannels = numberOfChannels
        self.linearPCMBitDepth = linearPCMBitDepth
    }
}

enum IOSAudioRecorderError: LocalizedError, Equatable {
    case microphonePermissionDenied
    case alreadyRecording
    case notRecording
    case failedToStart

    var errorDescription: String? {
        switch self {
        case .microphonePermissionDenied:
            return "Microphone permission is required before recording."
        case .alreadyRecording:
            return "A recording is already in progress."
        case .notRecording:
            return "No recording is currently in progress."
        case .failedToStart:
            return "The iOS audio recorder could not start."
        }
    }
}

@MainActor
final class IOSAudioRecorder: NSObject, AVAudioRecorderDelegate {
    private let configuration: IOSAudioRecorderConfiguration
    private var recorder: AVAudioRecorder?
    private var recordingStartedAt: Date?
    private(set) var recordingURL: URL?

    /// Called on the main actor when the OS stops or interrupts an active
    /// recording unexpectedly (phone call, Siri, audio session interruption, etc.).
    var onInterrupted: (() -> Void)?

    init(configuration: IOSAudioRecorderConfiguration = IOSAudioRecorderConfiguration()) {
        self.configuration = configuration
        super.init()
    }

    var isRecording: Bool {
        recorder?.isRecording == true
    }

    func startRecording() throws {
        guard !isRecording else { throw IOSAudioRecorderError.alreadyRecording }
        guard AVCaptureDevice.authorizationStatus(for: .audio) == .authorized else {
            throw IOSAudioRecorderError.microphonePermissionDenied
        }

        let session = AVAudioSession.sharedInstance()
        try session.setCategory(.playAndRecord, mode: .spokenAudio, options: [.defaultToSpeaker, .allowBluetooth])
        try session.setActive(true)

        let url = Self.makeRecordingURL()
        let settings: [String: Any] = [
            AVFormatIDKey: Int(kAudioFormatLinearPCM),
            AVSampleRateKey: configuration.sampleRate,
            AVNumberOfChannelsKey: configuration.numberOfChannels,
            AVLinearPCMBitDepthKey: configuration.linearPCMBitDepth,
            AVLinearPCMIsFloatKey: false,
            AVLinearPCMIsBigEndianKey: false,
            AVEncoderAudioQualityKey: AVAudioQuality.high.rawValue
        ]

        let newRecorder = try AVAudioRecorder(url: url, settings: settings)
        newRecorder.delegate = self
        newRecorder.isMeteringEnabled = true
        newRecorder.prepareToRecord()
        guard newRecorder.record() else {
            try? session.setActive(false, options: [.notifyOthersOnDeactivation])
            throw IOSAudioRecorderError.failedToStart
        }

        recorder = newRecorder
        recordingURL = url
        recordingStartedAt = Date()
    }

    func stopRecording() throws -> IOSAudioRecording {
        guard let activeRecorder = recorder, activeRecorder.isRecording else {
            throw IOSAudioRecorderError.notRecording
        }

        let url = activeRecorder.url
        let startedAt = recordingStartedAt ?? Date()
        activeRecorder.stop()
        recorder = nil
        recordingStartedAt = nil

        try AVAudioSession.sharedInstance().setActive(false, options: [.notifyOthersOnDeactivation])

        return IOSAudioRecording(
            recordingURL: url,
            durationSeconds: max(0, Date().timeIntervalSince(startedAt)),
            sampleRate: configuration.sampleRate,
            numberOfChannels: configuration.numberOfChannels
        )
    }

    // MARK: - AVAudioRecorderDelegate

    nonisolated func audioRecorderDidFinishRecording(_ recorder: AVAudioRecorder, successfully flag: Bool) {
        // Called when the recorder stops for any reason, including OS interruption.
        // If !flag the OS stopped it (phone call, Siri, etc.) — notify the model.
        guard !flag else { return }
        Task { @MainActor in
            self.recorder = nil
            self.recordingStartedAt = nil
            self.onInterrupted?()
        }
    }

    nonisolated func audioRecorderEncodeErrorDidOccur(_ recorder: AVAudioRecorder, error: Error?) {
        Task { @MainActor in
            self.recorder = nil
            self.recordingStartedAt = nil
            self.onInterrupted?()
        }
    }

    private static func makeRecordingURL() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("autowhisper-ios-\(UUID().uuidString)")
            .appendingPathExtension("caf")
    }
}

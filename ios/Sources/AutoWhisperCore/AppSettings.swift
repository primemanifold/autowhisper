import Foundation

public struct AutoWhisperSettings: Equatable, Sendable {
    public var defaultModel: ModelDescriptor
    public var recording: RecordingSettings
    public var outputMode: OutputMode
    public var privacy: PrivacySettings
    public var features: PlatformFeatureAvailability

    public init(
        defaultModel: ModelDescriptor,
        recording: RecordingSettings,
        outputMode: OutputMode,
        privacy: PrivacySettings,
        features: PlatformFeatureAvailability
    ) {
        self.defaultModel = defaultModel
        self.recording = recording
        self.outputMode = outputMode
        self.privacy = privacy
        self.features = features
    }

    public static let mobileDefaults = AutoWhisperSettings(
        defaultModel: ModelCatalog.mobileDefault.recommended,
        recording: .whisperCompatible,
        outputMode: .copyAndShare,
        privacy: PrivacySettings(localTranscriptionOnly: true),
        features: .ios
    )
}

public struct RecordingSettings: Equatable, Sendable {
    public var sampleRate: Int
    public var channelCount: Int
    public var maximumDurationSeconds: Double

    public init(sampleRate: Int, channelCount: Int, maximumDurationSeconds: Double) {
        self.sampleRate = sampleRate
        self.channelCount = channelCount
        self.maximumDurationSeconds = maximumDurationSeconds
    }

    public static let whisperCompatible = RecordingSettings(
        sampleRate: 16_000,
        channelCount: 1,
        maximumDurationSeconds: 60
    )
}

public enum OutputMode: Equatable, Sendable {
    case copyAndShare
}

public struct PrivacySettings: Equatable, Sendable {
    public var localTranscriptionOnly: Bool

    public init(localTranscriptionOnly: Bool) {
        self.localTranscriptionOnly = localTranscriptionOnly
    }
}

public struct PlatformFeatureAvailability: Equatable, Sendable {
    public var globalHotkeysAvailable: Bool
    public var arbitraryTextInsertionAvailable: Bool

    public init(globalHotkeysAvailable: Bool, arbitraryTextInsertionAvailable: Bool) {
        self.globalHotkeysAvailable = globalHotkeysAvailable
        self.arbitraryTextInsertionAvailable = arbitraryTextInsertionAvailable
    }

    public static let ios = PlatformFeatureAvailability(
        globalHotkeysAvailable: false,
        arbitraryTextInsertionAvailable: false
    )
}

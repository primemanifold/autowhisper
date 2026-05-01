import Foundation

public struct PermissionGate: Equatable, Sendable {
    public var microphone: PermissionStatus
    public var speechRecognition: PermissionStatus
    public var globalHotkeys: PermissionStatus
    public var arbitraryTextInsertion: PermissionStatus

    public init(
        microphone: PermissionStatus,
        speechRecognition: PermissionStatus,
        globalHotkeys: PermissionStatus,
        arbitraryTextInsertion: PermissionStatus
    ) {
        self.microphone = microphone
        self.speechRecognition = speechRecognition
        self.globalHotkeys = globalHotkeys
        self.arbitraryTextInsertion = arbitraryTextInsertion
    }

    public var canRecord: Bool {
        microphone == .granted
    }

    public var blockingMessages: [String] {
        var messages: [String] = []
        if microphone == .denied {
            messages.append("Microphone access is required to record dictation.")
        }
        return messages
    }

    public var unavailableCapabilityMessages: [String] {
        var messages: [String] = []
        if globalHotkeys == .unavailableOnIOS {
            messages.append("iOS does not allow global hotkeys for third-party apps.")
        }
        if arbitraryTextInsertion == .unavailableOnIOS {
            messages.append("iOS does not allow third-party apps to inject text into arbitrary apps.")
        }
        return messages
    }
}

public enum PermissionStatus: Equatable, Sendable {
    case unknown
    case granted
    case denied
    case notRequired
    case unavailableOnIOS
}

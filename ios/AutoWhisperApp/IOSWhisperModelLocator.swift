import Foundation
import AutoWhisperCore

enum IOSWhisperModelLocatorError: LocalizedError, Equatable {
    case missingBundledModel(String)

    var errorDescription: String? {
        switch self {
        case .missingBundledModel(let filename):
            return "Model not bundled yet: \(filename). Add it under AutoWhisperApp/Models before enabling real iOS Whisper inference."
        }
    }
}

final class IOSWhisperModelLocator: @unchecked Sendable {
    private let bundle: Bundle

    init(bundle: Bundle = .main) {
        self.bundle = bundle
    }

    func url(for model: ModelDescriptor) throws -> URL {
        let resourceName = model.bundleResourceName
        guard let url = bundle.url(forResource: resourceName, withExtension: "bin", subdirectory: "Models") else {
            throw IOSWhisperModelLocatorError.missingBundledModel(model.ggmlFilename)
        }
        return url
    }
}

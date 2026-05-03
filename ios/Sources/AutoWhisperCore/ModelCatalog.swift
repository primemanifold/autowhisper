import Foundation

public struct ModelCatalog: Equatable, Sendable {
    public var recommended: ModelDescriptor
    public var supported: [ModelDescriptor]

    public init(recommended: ModelDescriptor, supported: [ModelDescriptor]) {
        self.recommended = recommended
        self.supported = supported
    }

    public static let mobileDefault: ModelCatalog = {
        let tinyEnglish = ModelDescriptor(
            id: "tiny.en",
            displayName: "Tiny English",
            approximateSizeMB: 75,
            storage: .bundledResource,
            expectedUse: "Fastest local first-run experience on iPhone.",
            ggmlFilename: "ggml-tiny.en.bin"
        )
        let baseEnglish = ModelDescriptor(
            id: "base.en",
            displayName: "Base English",
            approximateSizeMB: 142,
            storage: .bundledResource,
            expectedUse: "Higher quality bundled option for newer devices.",
            ggmlFilename: "ggml-base.en.bin"
        )
        return ModelCatalog(recommended: tinyEnglish, supported: [tinyEnglish, baseEnglish])
    }()
}

public struct ModelDescriptor: Equatable, Sendable {
    public var id: String
    public var displayName: String
    public var approximateSizeMB: Int
    public var storage: ModelStorage
    public var expectedUse: String
    public var ggmlFilename: String
    public var bundleResourceName: String {
        String(ggmlFilename.split(separator: ".").dropLast().joined(separator: "."))
    }

    public init(
        id: String,
        displayName: String,
        approximateSizeMB: Int,
        storage: ModelStorage,
        expectedUse: String,
        ggmlFilename: String
    ) {
        self.id = id
        self.displayName = displayName
        self.approximateSizeMB = approximateSizeMB
        self.storage = storage
        self.expectedUse = expectedUse
        self.ggmlFilename = ggmlFilename
    }
}

public enum ModelStorage: Equatable, Sendable {
    case bundledResource
}

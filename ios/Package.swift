// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AutoWhisperIOS",
    platforms: [
        .iOS(.v16),
        .macOS(.v14),
    ],
    products: [
        .library(name: "AutoWhisperCore", targets: ["AutoWhisperCore"]),
        .executable(name: "AutoWhisperCoreChecks", targets: ["AutoWhisperCoreChecks"]),
    ],
    targets: [
        .target(name: "AutoWhisperCore"),
        .executableTarget(name: "AutoWhisperCoreChecks", dependencies: ["AutoWhisperCore"]),
    ]
)

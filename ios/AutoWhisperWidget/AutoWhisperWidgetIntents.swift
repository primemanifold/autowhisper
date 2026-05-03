import AppIntents

struct OpenRecorderIntent: AppIntent {
    static let title: LocalizedStringResource = "Open AutoWhisper to record"
    static let description = IntentDescription("Opens the AutoWhisper foreground app so microphone recording stays explicit and user controlled.")
    static let openAppWhenRun = true

    @MainActor
    func perform() async throws -> some IntentResult {
        .result()
    }
}

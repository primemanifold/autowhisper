import AppKit
import AVFoundation
import SwiftUI
import WebKit

struct ConfigDraft {
    var hotkeyMode = "push_to_talk"
    var trigger = "shift+super"
    var cancel = "esc"
    var modelSize = "distil-small.en"
    var modelDevice = "auto"
    var computeType = "bfloat16"
    var language = "en"
    var autoPaste = true
    var copyToClipboard = true
    var outputMethod = "inject"
    var endingAction = "none"
    var vadEnabled = true
    var vadThreshold = 0.5
    var muteOtherApps = false
    var feedbackEnabled = true
    var feedbackVolume = 0.3
    var trayEnabled = true
    var logLevel = "info"
}

final class ConfigStore: ObservableObject {
    @Published var draft = ConfigDraft()
    @Published var status = "Loaded"
    @Published var errorMessage: String?

    let configPath: String
    private var originalText = ""

    init(configPath: String) {
        self.configPath = configPath
        load()
    }

    func load() {
        do {
            originalText = try String(contentsOfFile: configPath, encoding: .utf8)
            draft = Self.parse(originalText)
            status = "Loaded \(URL(fileURLWithPath: configPath).lastPathComponent)"
            errorMessage = nil
        } catch {
            status = "Could not load config"
            errorMessage = error.localizedDescription
        }
    }

    func save() {
        do {
            var text = originalText
            let values: [(String, String, String)] = [
                ("hotkeys", "mode", quoted(draft.hotkeyMode)),
                ("hotkeys", "trigger", arrayValue(draft.trigger)),
                ("hotkeys", "cancel", arrayValue(draft.cancel)),
                ("model", "size", quoted(draft.modelSize)),
                ("model", "device", quoted(draft.modelDevice)),
                ("model", "compute_type", quoted(draft.computeType)),
                ("model", "language", quoted(draft.language)),
                ("output", "auto_paste", boolValue(draft.autoPaste)),
                ("output", "also_copy_to_clipboard", boolValue(draft.copyToClipboard)),
                ("output", "method", quoted(draft.outputMethod)),
                ("output", "ending_action", quoted(draft.endingAction)),
                ("audio", "vad_enabled", boolValue(draft.vadEnabled)),
                ("audio", "vad_threshold", numberValue(draft.vadThreshold)),
                ("audio", "mute_other_apps", boolValue(draft.muteOtherApps)),
                ("feedback", "enabled", boolValue(draft.feedbackEnabled)),
                ("feedback", "volume", numberValue(draft.feedbackVolume)),
                ("tray", "enabled", boolValue(draft.trayEnabled)),
                ("daemon", "log_level", quoted(draft.logLevel)),
            ]
            for (section, key, value) in values {
                text = Self.replacing(section: section, key: key, value: value, in: text)
            }
            try text.write(toFile: configPath, atomically: true, encoding: .utf8)
            originalText = text
            status = "Saved"
            errorMessage = nil
        } catch {
            status = "Save failed"
            errorMessage = error.localizedDescription
        }
    }

    private func quoted(_ value: String) -> String {
        "'" + value.replacingOccurrences(of: "'", with: "") + "'"
    }

    private func boolValue(_ value: Bool) -> String { value ? "true" : "false" }
    private func numberValue(_ value: Double) -> String { String(format: "%.2f", value) }

    private func arrayValue(_ value: String) -> String {
        let parts = value.split(separator: ",").map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }.filter { !$0.isEmpty }
        return "[ " + parts.map { "'\($0.replacingOccurrences(of: "'", with: ""))'" }.joined(separator: ", ") + " ]"
    }

    static func parse(_ text: String) -> ConfigDraft {
        var draft = ConfigDraft()
        var section = ""
        for rawLine in text.split(separator: "\n", omittingEmptySubsequences: false) {
            let line = rawLine.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("[") && line.hasSuffix("]") {
                section = String(line.dropFirst().dropLast())
                continue
            }
            guard let equals = line.firstIndex(of: "=") else { continue }
            let key = line[..<equals].trimmingCharacters(in: .whitespaces)
            let value = line[line.index(after: equals)...].trimmingCharacters(in: .whitespaces)
            let clean = scalar(value)
            switch (section, key) {
            case ("hotkeys", "mode"): draft.hotkeyMode = clean
            case ("hotkeys", "trigger"): draft.trigger = arrayText(value)
            case ("hotkeys", "cancel"): draft.cancel = arrayText(value)
            case ("model", "size"): draft.modelSize = clean
            case ("model", "device"): draft.modelDevice = clean
            case ("model", "compute_type"): draft.computeType = clean
            case ("model", "language"): draft.language = clean
            case ("output", "auto_paste"): draft.autoPaste = bool(value)
            case ("output", "also_copy_to_clipboard"): draft.copyToClipboard = bool(value)
            case ("output", "method"): draft.outputMethod = clean
            case ("output", "ending_action"): draft.endingAction = clean
            case ("audio", "vad_enabled"): draft.vadEnabled = bool(value)
            case ("audio", "vad_threshold"): draft.vadThreshold = Double(clean) ?? draft.vadThreshold
            case ("audio", "mute_other_apps"): draft.muteOtherApps = bool(value)
            case ("feedback", "enabled"): draft.feedbackEnabled = bool(value)
            case ("feedback", "volume"): draft.feedbackVolume = Double(clean) ?? draft.feedbackVolume
            case ("tray", "enabled"): draft.trayEnabled = bool(value)
            case ("daemon", "log_level"): draft.logLevel = clean
            default: break
            }
        }
        return draft
    }

    static func replacing(section targetSection: String, key targetKey: String, value: String, in text: String) -> String {
        var section = ""
        var lines = text.split(separator: "\n", omittingEmptySubsequences: false).map(String.init)
        for index in lines.indices {
            let trimmed = lines[index].trimmingCharacters(in: .whitespaces)
            if trimmed.hasPrefix("[") && trimmed.hasSuffix("]") {
                section = String(trimmed.dropFirst().dropLast())
                continue
            }
            guard section == targetSection, let equals = trimmed.firstIndex(of: "=") else { continue }
            let key = trimmed[..<equals].trimmingCharacters(in: .whitespaces)
            if key == targetKey {
                let indent = String(lines[index].prefix { $0 == " " || $0 == "\t" })
                lines[index] = "\(indent)\(targetKey) = \(value)"
                return lines.joined(separator: "\n")
            }
        }
        return text + "\n[\(targetSection)]\n\(targetKey) = \(value)\n"
    }

    private static func scalar(_ value: String) -> String {
        value.split(separator: "#", maxSplits: 1).first.map(String.init)?
            .trimmingCharacters(in: .whitespaces)
            .trimmingCharacters(in: CharacterSet(charactersIn: "'\"")) ?? ""
    }

    private static func bool(_ value: String) -> Bool { scalar(value).lowercased() == "true" }

    private static func arrayText(_ value: String) -> String {
        value.replacingOccurrences(of: "[", with: "")
            .replacingOccurrences(of: "]", with: "")
            .split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines).trimmingCharacters(in: CharacterSet(charactersIn: "'\"")) }
            .filter { !$0.isEmpty }
            .joined(separator: ", ")
    }
}

struct OnboardingView: View {
    @ObservedObject var store: ConfigStore
    let setupError: String?
    @State private var modelStatus = "Not downloaded yet"
    @State private var isDownloading = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            VStack(alignment: .leading, spacing: 6) {
                Text("First-run setup").font(.title2.bold())
                Text("Finish these one-time steps before AutoWhisper can listen globally and type into other apps.")
                    .foregroundStyle(.secondary)
                if let setupError, !setupError.isEmpty {
                    Label(setupError, systemImage: "exclamationmark.triangle.fill")
                        .foregroundStyle(.orange)
                }
            }

            HStack(alignment: .top, spacing: 12) {
                setupCard(title: "1. Download the local model",
                          subtitle: "Recommended: \(store.draft.modelSize). This can be a large download, but stays local after it finishes.",
                          systemImage: "arrow.down.circle") {
                    Button(isDownloading ? "Downloading…" : "Download recommended model") {
                        downloadRecommendedModel()
                    }
                    .disabled(isDownloading)
                    Text(modelStatus).font(.caption).foregroundStyle(.secondary)
                }

                setupCard(title: "2. Enable Mac permissions",
                          subtitle: "macOS requires explicit trust for the global hotkey, text injection, and microphone.",
                          systemImage: "lock.shield") {
                    Button("Open Input Monitoring") { openPrivacyPane("Privacy_ListenEvent") }
                    Button("Open Accessibility") { openPrivacyPane("Privacy_Accessibility") }
                    Button("Request Microphone") {
                        AVCaptureDevice.requestAccess(for: .audio) { _ in }
                        openPrivacyPane("Privacy_Microphone")
                    }
                }
            }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: 22, style: .continuous).fill(Color.accentColor.opacity(0.08)))
        .overlay(RoundedRectangle(cornerRadius: 22, style: .continuous).stroke(Color.accentColor.opacity(0.25), lineWidth: 1))
    }

    private func setupCard<Content: View>(title: String, subtitle: String, systemImage: String,
                                          @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Label(title, systemImage: systemImage).font(.headline)
            Text(subtitle).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            VStack(alignment: .leading, spacing: 8) { content() }
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .topLeading)
        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(Color(nsColor: .controlBackgroundColor)))
    }

    private func openPrivacyPane(_ anchor: String) {
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?\(anchor)") {
            NSWorkspace.shared.open(url)
        }
    }

    private func downloadRecommendedModel() {
        guard !isDownloading else { return }
        isDownloading = true
        modelStatus = "Starting download…"
        let model = store.draft.modelSize
        DispatchQueue.global(qos: .userInitiated).async {
            let process = Process()
            let pipe = Pipe()
            let outputHandle = pipe.fileHandleForReading
            let helperDir = Bundle.main.executableURL?.deletingLastPathComponent()
            process.executableURL = helperDir?.appendingPathComponent("autowhisper") ?? URL(fileURLWithPath: "/usr/local/bin/autowhisper")
            process.arguments = ["model", "download", model]
            process.standardOutput = pipe
            process.standardError = pipe

            let outputLock = NSLock()
            var capturedOutput = ""
            outputHandle.readabilityHandler = { handle in
                let data = handle.availableData
                guard !data.isEmpty else { return }
                let chunk = String(data: data, encoding: .utf8) ?? ""
                if !chunk.isEmpty {
                    outputLock.lock()
                    capturedOutput.append(chunk)
                    outputLock.unlock()
                    let trimmed = chunk.trimmingCharacters(in: .whitespacesAndNewlines)
                    if !trimmed.isEmpty {
                        DispatchQueue.main.async {
                            modelStatus = trimmed.components(separatedBy: .newlines).last ?? "Downloading…"
                        }
                    }
                }
            }

            do {
                try process.run()
                process.waitUntilExit()
                outputHandle.readabilityHandler = nil
                _ = outputHandle.readDataToEndOfFile()
                let finalOutput: String
                outputLock.lock()
                finalOutput = capturedOutput
                outputLock.unlock()
                DispatchQueue.main.async {
                    isDownloading = false
                    modelStatus = process.terminationStatus == 0 ? "Model is ready" : "Download failed: \(finalOutput.prefix(180))"
                }
            } catch {
                outputHandle.readabilityHandler = nil
                DispatchQueue.main.async {
                    isDownloading = false
                    modelStatus = "Download failed: \(error.localizedDescription)"
                }
            }
        }
    }
}

struct SettingsView: View {
    @ObservedObject var store: ConfigStore
    let setupError: String?

    var body: some View {
        HStack(spacing: 0) {
            sidebar
            Divider()
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    hero
                    OnboardingView(store: store, setupError: setupError)
                    section("Dictation", subtitle: "How AutoWhisper listens and cancels.") {
                        Picker("Mode", selection: $store.draft.hotkeyMode) {
                            Text("Push to talk").tag("push_to_talk")
                            Text("Toggle").tag("toggle")
                        }
                        TextField("Trigger hotkey", text: $store.draft.trigger)
                        TextField("Cancel hotkey", text: $store.draft.cancel)
                    }
                    section("Model & performance", subtitle: "Local Whisper model and compute defaults.") {
                        Picker("Model", selection: $store.draft.modelSize) {
                            ForEach(["tiny.en", "base.en", "small.en", "distil-small.en", "medium.en", "large-v3"], id: \.self) { Text($0).tag($0) }
                        }
                        Picker("Device", selection: $store.draft.modelDevice) {
                            ForEach(["auto", "cpu", "cuda"], id: \.self) { Text($0).tag($0) }
                        }
                        TextField("Language", text: $store.draft.language)
                    }
                    section("Output", subtitle: "Where dictated text goes after transcription.") {
                        Toggle("Auto paste", isOn: $store.draft.autoPaste)
                        Toggle("Also copy to clipboard", isOn: $store.draft.copyToClipboard)
                        Picker("Method", selection: $store.draft.outputMethod) {
                            Text("Inject text").tag("inject")
                            Text("Clipboard").tag("clipboard")
                        }
                    }
                    section("Audio & feedback", subtitle: "Noise handling and confirmation sounds.") {
                        Toggle("Voice activity detection", isOn: $store.draft.vadEnabled)
                        Slider(value: $store.draft.vadThreshold, in: 0.1...0.95) { Text("VAD threshold") }
                        Toggle("Mute other apps while recording", isOn: $store.draft.muteOtherApps)
                        Toggle("Feedback sounds", isOn: $store.draft.feedbackEnabled)
                        Slider(value: $store.draft.feedbackVolume, in: 0...1) { Text("Feedback volume") }
                    }
                    section("Advanced", subtitle: "Tray and logging.") {
                        Toggle("Show menu-bar item", isOn: $store.draft.trayEnabled)
                        Picker("Log level", selection: $store.draft.logLevel) {
                            ForEach(["trace", "debug", "info", "warn", "error"], id: \.self) { Text($0).tag($0) }
                        }
                    }
                }
                .padding(28)
            }
            .background(Color(nsColor: .windowBackgroundColor))
        }
        .frame(minWidth: 980, minHeight: 720)
    }

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack(spacing: 12) {
                Image(systemName: "mic.circle.fill").font(.system(size: 34)).foregroundStyle(.blue)
                VStack(alignment: .leading) {
                    Text("AutoWhisper").font(.title3.bold())
                    Text("Native settings").foregroundStyle(.secondary)
                }
            }
            Divider()
            Label("Dictation", systemImage: "keyboard")
            Label("Model", systemImage: "cpu")
            Label("Output", systemImage: "text.cursor")
            Label("Audio", systemImage: "waveform")
            Label("Advanced", systemImage: "gearshape")
            Spacer()
            Text(store.configPath).font(.caption2).foregroundStyle(.secondary).lineLimit(4)
        }
        .padding(22)
        .frame(width: 245, alignment: .leading)
        .background(.thinMaterial)
    }

    private var hero: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Settings").font(.system(size: 34, weight: .bold, design: .rounded))
            Text("A lean native Mac control panel for local dictation. No browser tab, no webview.")
                .foregroundStyle(.secondary)
            HStack {
                Label(store.status, systemImage: "checkmark.seal.fill").foregroundStyle(.green)
                if let message = store.errorMessage { Text(message).foregroundStyle(.red) }
                Spacer()
                Button("Reload") { store.load() }
                Button("Save changes") { store.save() }.buttonStyle(.borderedProminent)
            }
        }
    }

    private func section<Content: View>(_ title: String, subtitle: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 14) {
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.title2.bold())
                Text(subtitle).foregroundStyle(.secondary)
            }
            VStack(alignment: .leading, spacing: 12) { content() }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: 20, style: .continuous).fill(Color(nsColor: .controlBackgroundColor)))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).stroke(Color(nsColor: .separatorColor), lineWidth: 1))
    }
}

final class AutoWhisperSettingsApp: NSObject, NSApplicationDelegate {
    private var window: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        let args = CommandLine.arguments

        // --url mode: host the full web settings UI (themes, the cast, hotkey
        // capture, the permissions pane, the model card) in a real, front-most
        // app window — not a buried browser tab. This is the macOS settings
        // surface; the SwiftUI form below is only the first-run setup helper.
        if let index = args.firstIndex(of: "--url"), args.indices.contains(index + 1),
           let url = URL(string: args[index + 1]) {
            let webView = WKWebView(frame: NSRect(x: 0, y: 0, width: 1040, height: 760))
            webView.load(URLRequest(url: url))
            let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1040, height: 760),
                                  styleMask: [.titled, .closable, .miniaturizable, .resizable],
                                  backing: .buffered,
                                  defer: false)
            window.title = "AutoWhisper Settings"
            window.contentView = webView
            window.center()
            window.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            self.window = window
            return
        }

        let configPath: String
        let setupError: String?
        if let index = args.firstIndex(of: "--config"), args.indices.contains(index + 1) {
            configPath = args[index + 1]
        } else {
            configPath = FileManager.default.homeDirectoryForCurrentUser
                .appendingPathComponent(".config/autowhisper/config.toml").path
        }
        if let index = args.firstIndex(of: "--setup-error"), args.indices.contains(index + 1) {
            setupError = args[index + 1]
        } else {
            setupError = nil
        }

        let view = SettingsView(store: ConfigStore(configPath: configPath), setupError: setupError)
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1040, height: 760),
                              styleMask: [.titled, .closable, .miniaturizable, .resizable],
                              backing: .buffered,
                              defer: false)
        window.title = "AutoWhisper Settings"
        window.contentView = NSHostingView(rootView: view)
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        self.window = window
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

let app = NSApplication.shared
let delegate = AutoWhisperSettingsApp()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()

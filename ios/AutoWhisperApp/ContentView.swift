import AVFoundation
import SwiftUI
import AutoWhisperCore
#if canImport(UIKit)
import UIKit
#endif

@MainActor
final class AutoWhisperAppModel: ObservableObject {
    @Published var settings = AutoWhisperSettings.mobileDefaults
    @Published var permissionStatus: String = "Microphone permission not requested"
    @Published var recordingState: RecordingState = .idle
    @Published var transcriptText: String = ""
    @Published var errorMessage: String?
    @Published var quickRecordMessage: String?

    private let audioRecorder = IOSAudioRecorder()
    private let transcriber: IOSWhisperTranscribing
    private var isRecordingTransitionInFlight = false

    init(transcriber: IOSWhisperTranscribing = IOSPlaceholderWhisperTranscriber()) {
        self.transcriber = transcriber
    }

    func updateMicrophonePermissionStatus() {
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .authorized:
            permissionStatus = "Microphone permission granted"
        case .denied, .restricted:
            permissionStatus = "Microphone permission denied — enable it in Settings to record."
        case .notDetermined:
            permissionStatus = "Microphone permission not requested"
        @unknown default:
            permissionStatus = "Microphone permission unavailable"
        }
    }

    @discardableResult
    func requestMicrophonePermission() async -> Bool {
        let granted = await AVCaptureDevice.requestAccess(for: .audio)
        permissionStatus = granted ? "Microphone permission granted" : "Microphone permission denied — enable it in Settings to record."
        return granted
    }

    func hasMicrophonePermissionForRecording() async -> Bool {
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .authorized:
            updateMicrophonePermissionStatus()
            return true
        case .notDetermined:
            let granted = await requestMicrophonePermission()
            if !granted {
                errorMessage = "Microphone permission is required before recording. Enable it in Settings to use AutoWhisper on iPhone."
            }
            return granted
        case .denied, .restricted:
            updateMicrophonePermissionStatus()
            errorMessage = "Microphone permission is required before recording. Enable it in Settings to use AutoWhisper on iPhone."
            return false
        @unknown default:
            updateMicrophonePermissionStatus()
            errorMessage = "Microphone permission is unavailable on this device."
            return false
        }
    }

    func handleDeepLink(_ url: URL) async {
        guard url.scheme == "autowhisper", url.host == "record" else { return }
        quickRecordMessage = "Quick Record opened from widget. Recording starts only while AutoWhisper is foregrounded."
        if AVCaptureDevice.authorizationStatus(for: .audio) == .notDetermined {
            await requestMicrophonePermission()
        } else {
            updateMicrophonePermissionStatus()
        }

        guard AVCaptureDevice.authorizationStatus(for: .audio) == .authorized else { return }
        guard recordingState == .idle else { return }

        errorMessage = nil
        do {
            try audioRecorder.startRecording()
            recordingState = .recording
            transcriptText = ""
        } catch {
            recordingState = .failed
            errorMessage = "Quick Record failed: \(error.localizedDescription)"
        }
    }

    func toggleRecording() async {
        guard recordingState != .preparingTranscript else { return }
        guard !isRecordingTransitionInFlight else { return }
        isRecordingTransitionInFlight = true
        defer { isRecordingTransitionInFlight = false }

        errorMessage = nil
        do {
            if recordingState == .recording {
                recordingState = .preparingTranscript
                let recording = try audioRecorder.stopRecording()
                transcriptText = try await transcriber.transcribe(recording: recording)
                recordingState = .idle
            } else {
                recordingState = .preparingRecording
                guard await hasMicrophonePermissionForRecording() else {
                    recordingState = .idle
                    return
                }
                try audioRecorder.startRecording()
                recordingState = .recording
                transcriptText = ""
                updateMicrophonePermissionStatus()
            }
        } catch {
            if recordingState == .recording {
                recordingState = .idle
            } else {
                recordingState = .failed
            }
            updateMicrophonePermissionStatus()
            errorMessage = "Recording failed: \(error.localizedDescription)"
        }
    }

    func copyTranscript() {
        #if canImport(UIKit)
        UIPasteboard.general.string = transcriptText
        #endif
    }
}

struct ContentView: View {
    @StateObject private var model = AutoWhisperAppModel()

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    hero
                    permissionCard
                    recorderCard
                    transcriptCard
                    limitationsCard
                }
                .padding(24)
            }
            .navigationTitle("AutoWhisper")
            .onOpenURL { url in
                Task { await model.handleDeepLink(url) }
            }
        }
    }

    private var hero: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Local voice notes for iPhone")
                .font(.largeTitle.bold())
            Text("Record in the foreground, decode locally, then copy or share the bridge result. Real Whisper transcription is still the next iOS slice.")
                .font(.body)
                .foregroundStyle(.secondary)
            Label("Recommended model: \(model.settings.defaultModel.id)", systemImage: "waveform")
                .font(.callout.weight(.semibold))
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var permissionCard: some View {
        GroupBox("Microphone") {
            VStack(alignment: .leading, spacing: 12) {
                Text(model.permissionStatus)
                Button("Request Microphone Permission") {
                    Task { await model.requestMicrophonePermission() }
                }
                .buttonStyle(.bordered)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var recordingButtonTitle: String {
        switch model.recordingState {
        case .recording:
            return "Stop & Decode Audio"
        case .preparingRecording:
            return "Preparing Recording…"
        case .preparingTranscript:
            return "Decoding Audio…"
        default:
            return "Start Recording"
        }
    }

    private var recorderCard: some View {
        GroupBox("Recorder") {
            VStack(alignment: .leading, spacing: 12) {
                Text("16 kHz mono recording target for Whisper-compatible audio.")
                    .foregroundStyle(.secondary)
                Button(recordingButtonTitle) {
                    Task { await model.toggleRecording() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(model.recordingState == .preparingRecording || model.recordingState == .preparingTranscript)
                if let errorMessage = model.errorMessage {
                    Text(errorMessage)
                        .foregroundStyle(.red)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var transcriptCard: some View {
        GroupBox("Transcript") {
            VStack(alignment: .leading, spacing: 12) {
                Text(model.transcriptText.isEmpty ? "Your decoded bridge summary appears here after recording." : model.transcriptText)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
                    .background(Color.secondary.opacity(0.12), in: RoundedRectangle(cornerRadius: 12))
                HStack {
                    Button("Copy Transcript") { model.copyTranscript() }
                        .disabled(model.transcriptText.isEmpty)
                    ShareLink(item: model.transcriptText) {
                        Text("Share Transcript")
                    }
                    .disabled(model.transcriptText.isEmpty)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var limitationsCard: some View {
        GroupBox("iOS limits") {
            VStack(alignment: .leading, spacing: 8) {
                Text("iOS does not allow global hotkeys for third-party apps.")
                Text("iOS does not allow arbitrary text injection into other apps.")
                Text("Use copy/share today; keyboard or share extensions would be separate future surfaces.")
            }
            .font(.footnote)
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

#Preview {
    ContentView()
}

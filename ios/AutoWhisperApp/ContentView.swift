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

    private let engine = FakeTranscriptionEngine(result: "This is a local AutoWhisper iOS transcript preview.")
    private let audioRecorder = IOSAudioRecorder()
    private var session: RecordingSession?

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

    func requestMicrophonePermission() async {
        let granted = await AVCaptureDevice.requestAccess(for: .audio)
        permissionStatus = granted ? "Microphone permission granted" : "Microphone permission denied — enable it in Settings to record."
    }

    func toggleRecording() async {
        errorMessage = nil
        do {
            if recordingState == .recording, let session {
                let recording = try audioRecorder.stopRecording()
                let transcript = try await session.stopAndTranscribe(audio: AudioFixture(samples: [], sampleRate: Int(recording.sampleRate)))
                transcriptText = """
                \(transcript.text)

                Recorded \(recording.durationSeconds.formatted(.number.precision(.fractionLength(1))))s of foreground iOS audio.
                File: \(recording.recordingURL.lastPathComponent)
                Local Whisper inference bridge is the next slice.
                """
                recordingState = await session.state
                self.session = nil
            } else {
                let newSession = RecordingSession(engine: engine)
                try await newSession.startRecording()
                try audioRecorder.startRecording()
                session = newSession
                recordingState = await newSession.state
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
        }
    }

    private var hero: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Local voice notes for iPhone")
                .font(.largeTitle.bold())
            Text("Record in the foreground, transcribe locally, then copy or share the transcript. No desktop daemon promises on iOS.")
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

    private var recorderCard: some View {
        GroupBox("Recorder") {
            VStack(alignment: .leading, spacing: 12) {
                Text("16 kHz mono recording target for Whisper-compatible audio.")
                    .foregroundStyle(.secondary)
                Button(model.recordingState == .recording ? "Stop & Transcribe" : "Start Recording") {
                    Task { await model.toggleRecording() }
                }
                .buttonStyle(.borderedProminent)
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
                Text(model.transcriptText.isEmpty ? "Your transcript appears here after recording." : model.transcriptText)
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

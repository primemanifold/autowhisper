# AutoWhisper Platform Readiness Matrix

This matrix separates evidence from aspiration. Vocabulary: `ready`, `partial`, `build_proven`, `packaged`, `release_proven`, `unsupported`, `unverified`, `planned`.

## Summary table

| Platform | source_build | package | public_artifact | install_firstrun | microphone | quick_launch | text_output | model_mgmt | local_transcription | runtime_e2e | next_milestone |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| macOS | `ready` | `ready` | `release_proven` | `partial` | `partial` | `partial` | `partial` | `partial` | `partial` | `partial` | Validate downloaded public ZIP first-run and model-download flow on a clean machine. |
| Linux | `build_proven` | `packaged` | `unverified` | `partial` | `partial` | `partial` | `partial` | `partial` | `unverified` | `unverified` | Add documented X11 runtime smoke and PPA install evidence. |
| Windows | `build_proven` | `build_proven` | `unverified` | `unverified` | `build_proven` | `build_proven` | `build_proven` | `unverified` | `unverified` | `unverified` | Run a real Windows or Wine smoke before any runtime support language. |
| iOS | `ready` | `build_proven` | `planned` | `unverified` | `partial` | `partial` | `unverified` | `unverified` | `planned` | `unverified` | Prove the widget/deep-link flow and foreground recording on a physical device, then add real whisper.cpp output. |

## Evidence notes

### macOS

- macOS source_build `ready`: local Xcode/CMake build capability and app bundle tests exist. This does not prove a downloaded public release artifact works on a clean Mac.
- macOS package `ready`: app-bundle packaging and signing/notarization workflow exist. This does not prove every future release ZIP is valid.
- macOS public_artifact `release_proven`: v0.7.1 public ZIP exists at https://github.com/primemanifold/autowhisper/releases/download/v0.7.1/AutoWhisper-macOS-v0.7.1.zip. This does not prove later release fixes are public.
- macOS microphone `partial`: permission copy and app-bundle checks exist. This does not prove first-run microphone capture on every clean install.
- macOS runtime_e2e `partial`: local validation exists. This does not prove downloaded-artifact install, first-run, model download, microphone capture, and transcript output end-to-end.

### Linux

- Linux source_build `build_proven`: CMake and static tests exercise Linux paths. This does not prove PPA install or desktop-session runtime.
- Linux package `packaged`: PPA commands are documented. This does not prove the current PPA package installed and transcribed on this machine.
- Linux microphone `partial`: PulseAudio/X11 code paths exist. This does not prove Wayland text injection or microphone behavior in every desktop environment.
- Linux runtime_e2e `unverified`: no current evidence of install -> first run -> record -> text output from a fresh Linux host is recorded here.

### Windows

- Windows source_build `build_proven`: cross-build feasibility is known. This does not prove Windows runtime behavior.
- Windows package `build_proven`: build artifacts may be produced by cross-toolchains. This does not prove installer, signing, first-run, microphone permissions, or text output.
- Windows microphone `build_proven`: compile-level coverage can include audio abstractions. This does not prove microphone capture on Windows.
- Windows runtime_e2e `unverified`: runtime unproven; no Windows support claim should be made from cross-build evidence alone.

### iOS

- iOS source_build `ready`: Swift package checks and Xcode project generation exist. This does not prove App Store readiness.
- iOS package `build_proven`: simulator and generic device builds have passed. This does not prove physical-device install or TestFlight.
- iOS microphone `partial`: AVFoundation foreground recorder exists. This does not prove physical-device capture across permissions and interruptions.
- iOS quick_launch `partial`: WidgetKit/deep-link launcher exists. This does not prove a real widget tap on a physical device.
- iOS local_transcription `planned`: whisper.cpp bridge pending; placeholder seams exist only. This does not prove real transcript output.
- iOS runtime_e2e `unverified`: no physical-device record -> real whisper.cpp transcript -> share/copy proof yet.

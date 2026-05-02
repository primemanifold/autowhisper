# AutoWhisper Media Inventory

Media assets are evidence, not decoration. Do not add screenshots or videos until they are captured from the relevant platform.

| Asset | Platform | Status | Capture command / method | Caption boundary |
| --- | --- | --- | --- | --- |
| macOS first launch | macOS | TODO: capture | Screenshot after opening downloaded v0.7.1 app | Shows launch only; not runtime_e2e. |
| macOS permissions | macOS | TODO: capture | System Settings privacy panes | Shows Microphone, Accessibility, Input Monitoring setup. |
| Linux doctor | Linux | TODO: capture | `autowhisper doctor` on X11 | Shows diagnostics only; not Wayland support. |
| Linux PPA install | Linux | TODO: capture | Terminal recording of PPA install commands | Shows package install only. |
| Windows build | Windows | TODO: capture | Screenshot of successful cross-build logs | build_proven only; runtime unproven. |
| iOS widget opens app | iOS | TODO: capture | Simulator/physical-device widget tap screenshot | Widget opens foreground app; no direct widget recording. |
| iOS recording shell | iOS | TODO: capture | Foreground app recording screen | whisper.cpp bridge pending. |

## Local-first caption rule

Captions may say AutoWhisper is local-first and offline-oriented when describing shipped desktop transcription paths. Captions must not imply cloud transcription is required, and must not imply unsupported platform parity.

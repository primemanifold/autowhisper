# AutoWhisper Usage Guide

AutoWhisper is local-first voice-to-text. The core product is designed to run offline without sending transcription to a cloud service.

## Platform status at a glance

- macOS: public v0.7.1 ZIP is available; runtime_e2e remains partial until downloaded-artifact first-run and model flow are revalidated.
- Linux: PPA install path exists; validate runtime on X11 first. Wayland text injection may require different integration work.
- Windows: build_proven only; runtime unproven and coming soon.
- iOS: SwiftUI shell, foreground recorder, and widget opens foreground app are present; whisper.cpp bridge pending for real local transcription.

## macOS quick start

[Screenshot: TODO — see docs/media/README.md]

1. Download v0.7.1: https://github.com/primemanifold/autowhisper/releases/download/v0.7.1/AutoWhisper-macOS-v0.7.1.zip
2. Unzip and move `AutoWhisper.app` to `/Applications`.
3. Open AutoWhisper and grant Microphone, Accessibility, and Input Monitoring when prompted.
4. Use the configured push-to-talk shortcut and verify text output in a simple text field.

## Linux quick start

[Screenshot: TODO — see docs/media/README.md]

```bash
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update
sudo apt install autowhisper
autowhisper doctor
systemctl --user enable --now autowhisper
```

Prefer X11 for the first runtime smoke. On Wayland, hotkeys and text insertion are not yet release-proven.

## Windows status

Windows is `build_proven` only. Runtime unproven means there is no public Windows install or first-run claim yet.

## iOS status

The iOS app shell can build, record foreground audio, and expose a Quick Record widget that opens the foreground app. Real local transcription is not shipped yet; whisper.cpp bridge pending.

## Troubleshooting proof levels

- `source_build`: code compiles locally.
- `public_artifact`: a downloadable release exists.
- `install_firstrun`: a clean machine can install and open it.
- `runtime_e2e`: record audio, transcribe locally, and deliver text output.

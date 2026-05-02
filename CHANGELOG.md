# Changelog

## 0.7.2 — 2026-05-01

### Fixed
- macOS app bundles now include a generated `AutoWhisper.icns` resource and declare `CFBundleIconFile`, so Finder, Dock, and LaunchServices have a real app icon.
- The native first-run Settings helper now downloads models directly with `URLSession` into `~/.cache/whisper` instead of delegating to a child CLI process from the GUI context.
- Model download status now reports clearer success/failure states, including HTTP errors, missing output files, unexpectedly tiny downloads, and already-downloaded models.
- Corrected the displayed `distil-small.en` size from `~166MB` to `~320MB`.

### Validation
- Verified the generated icon resource is present in `AutoWhisper.app/Contents/Resources/AutoWhisper.icns` and declared as `CFBundleIconFile = AutoWhisper`.
- Verified a clean HOME model download writes `~/.cache/whisper/ggml-tiny.en.bin` with `77,704,715` bytes and reports `Model tiny.en ready!`.
- Local macOS CTest suite passed: 117/117.
- Static settings and macOS bundle asset tests passed: 27/27.
- macOS app smoke validation passed against the built bundle.

## 0.7.1 — 2026-05-01

### Fixed
- macOS installed-app first run now opens the native setup helper instead of silently exiting when required setup is missing, such as the default `distil-small.en` model.
- App-bundle launch now creates/copies a writable user config at `~/.config/autowhisper/config.toml` so installed builds are not dependent on the repository checkout.
- Native setup helper now presents first-run actions for model download and macOS permissions: Microphone, Input Monitoring, and Accessibility.

### Validation
- Reproduced the shipped `v0.7.0` ZIP from a fresh release download: Gatekeeper accepted, but clean first run exited `1`, created no user config, and stopped on missing `distil-small.en` without onboarding.
- Verified the fixed app-bundle first run on a clean HOME: exit `0`, user config created, and missing-model failure routed to setup/onboarding.
- Captured local screenshot-style evidence report for the broken download and fixed branch comparison.

## 0.7.0 — 2026-05-01

### Added
- First macOS application-bundle release path: default macOS CMake builds now emit `AutoWhisper.app`.
- Native macOS integration surface, including menu-bar identity, launchd/service scaffolding, permission diagnostics, microphone entitlement/purpose strings, and platform-specific hotkey/output/tray/doctor seams.
- Universal macOS app bundle proof for `arm64` + `x86_64`, targeting macOS 12.0+.
- Bundled default `config.toml` fallback so double-clicked app launches do not depend on repository working directory.
- Finder/LaunchServices launch handling so `AutoWhisper.app` starts the foreground daemon path when launched without a CLI subcommand.
- Developer ID signing, notarization, stapling, and Gatekeeper acceptance proof for the macOS `.app` artifact.
- Native SwiftUI settings helper bundled inside `AutoWhisper.app`, launched from the menu-bar Settings action instead of opening the browser UI.

### Changed
- macOS bundle signing now uses a secure timestamp for Developer ID distribution readiness.
- Desktop platform readiness is surfaced honestly through tested platform capability diagnostics.

### Validation
- Local macOS CTest suite passed: 117/117.
- Static settings and macOS bundle asset tests passed: 18/18.
- Developer ID-signed `AutoWhisper.app` was notarized, stapled, and accepted by Gatekeeper as `source=Notarized Developer ID`.

### Boundaries
- Windows runtime end-to-end behavior remains unclaimed until validated on a real Windows host/VM or proven compatible runtime.
- macOS release proof covers app bundle/signing/notarization/Gatekeeper acceptance; it does not yet claim final DMG/PKG installer UX, auto-update, or full hotkey→record→transcribe→insert product acceptance testing.

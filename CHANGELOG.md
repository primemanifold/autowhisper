# Changelog

## 0.7.0 — 2026-05-01

### Added
- First macOS application-bundle release path: default macOS CMake builds now emit `AutoWhisper.app`.
- Native macOS integration surface, including menu-bar identity, launchd/service scaffolding, permission diagnostics, microphone entitlement/purpose strings, and platform-specific hotkey/output/tray/doctor seams.
- Universal macOS app bundle proof for `arm64` + `x86_64`, targeting macOS 12.0+.
- Bundled default `config.toml` fallback so double-clicked app launches do not depend on repository working directory.
- Finder/LaunchServices launch handling so `AutoWhisper.app` starts the foreground daemon path when launched without a CLI subcommand.
- Developer ID signing, notarization, stapling, and Gatekeeper acceptance proof for the macOS `.app` artifact.

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

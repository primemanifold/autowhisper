# Changelog

## 0.8.0 — 2026-06-12

Truth & foundations (production plan M1), the intelligence layer's first
slice, the Echo companion, and a real Windows port.

### Added
- Multilingual model catalog (tiny/base/small/medium/large-v3-turbo) with `language = "auto"` detection; every model download is SHA-256 verified against pinned Hugging Face checksums, and corrupt caches self-heal.
- Transcript pipeline (`[formatting]`): filler-word removal, personal dictionary (`spoken => written`), and spoken commands ("new line", "new paragraph"), with 29 golden tests.
- Echo, the floating companion (`[avatar]`, opt-in): a Wispr-Flow-style floating button — click to start/stop dictation, live voice halo while listening, state captions (LISTENING/THINKING/WRITING), drag to reposition. Forms: echo, hermes, mnemosyne. One shared software renderer drives X11, Win32 layered windows, and NSPanel shells.
- Windows desktop integrations (closes #14): global hotkeys via a WH_KEYBOARD_LL low-level hook (push-to-talk, toggle, modifier-only chords) and text output via SendInput Unicode injection + CF_UNICODETEXT clipboard + synthesized paste. The exe is self-contained (static runtimes, no VC++ redistributable).
- Benchmark harness `autowhisper_bench` (model load, median latency, RTF, WER) with a nightly CI run; README performance claims are now measured.
- CI builds and uploads downloadable artifacts for Linux, macOS (binary + unsigned .app), and Windows (MSVC and MinGW) on every push; `docs/INSTALL-UNSIGNED.md` covers no-signing-key installs and first model download.
- Settings server hardening: per-session bearer token on all API routes and a loopback Host allowlist (DNS-rebinding defense).
- Neutral light/dark design system across the settings UI, landing page, and mobile design previews, with status-only motion gated behind `prefers-reduced-motion`.

### Changed
- X11 hotkey listener rewritten to the canonical synchronous XRecord pattern with a retried disable nudge and bounded (5s) shutdown; a dead listener connection no longer terminates the daemon. Covered by Xvfb+XTest integration tests.
- Config floats serialize as their shortest round-trip decimal in both TOML and the settings JSON API (0.05, not 0.05000000074505806).
- Web settings assets re-embed automatically when edited (CMake configure dependency).

### Fixed
- Broken `distil-medium.en` download URL and wrong catalog size claims.
- Settings UI layout overflow at phone widths.
- Settings-resume hotkey deadlock class closed (docs/hotkey-resume-deadlock.md).

### Validation
- 178/178 C++ tests and full static suites green on Linux and macOS CI; MSVC job builds and passes the native Windows test suite; MinGW artifact verified self-contained.
- Avatar runtime: full state script recorded under Xvfb (Linux) and under Wine running the actual Windows exe, pixel-identical output.
- End-to-end model download with checksum verification and corrupt-cache self-heal exercised against Hugging Face.
- Remaining honest gaps: live keystroke validation on physical Windows hardware; macOS runtime pass for the companion (compiles + tests green on CI; panel behavior needs a Mac).


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

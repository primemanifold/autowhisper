# TODOS

Deferred work. Items here are explicitly out of scope for current work but
tracked so they aren't lost.

## macOS port follow-ups (added 2026-04-20 by /autoplan)

- **Homebrew tap** — `brew install primemanifold/tap/autowhisper` for
  a zero-TCC-prompt CLI install. Parallel to the signed `.pkg`. Effort:
  S (CC ~20 min once the signed release binary exists).
- **Sparkle auto-update** — wire Sparkle 2 into the `.app` so releases
  self-update. Requires an EdDSA signing key and a published appcast.xml.
  Effort: M.
- **DMG installer** — drag-to-Applications alternative to `.pkg`. Useful
  for users who dislike `.pkg` postinstall scripts. Effort: S.
- **Mute-other-apps on macOS** — CoreAudio HAL tap to replicate the
  PulseAudio behavior. Complex and app-store-incompatible; likely needs a
  per-app volume approach via `AVAudioSession` instead. Effort: L.
- **First-run onboarding window** — a one-time popover that walks the user
  through granting Accessibility and Input Monitoring. Replaces the
  doctor-CLI path for GUI-first users. Effort: M (new Cocoa surface).
- **`os_log` / `log stream` bridging** — pipe spdlog writes through
  `os_log` so `log stream --subsystem us.primemanifold.autowhisper` works.
  Effort: M.
- **Windows port** — same pattern as mac (per-platform files in `*/platform/`).
  Hotkey via `RegisterHotKey` or `SetWindowsHookEx`, text via `SendInput`,
  tray via `Shell_NotifyIcon`, service via either a scheduled task or a
  Windows service. Effort: L.

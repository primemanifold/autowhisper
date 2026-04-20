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

## Tests / coverage (added 2026-04-20 by /ship coverage audit)

- **launchctl state-machine unit test** — extract the
  `current_label_loaded → matches_plist → bootstrap vs kickstart` decision
  in `src/service/service_macos.mm` into a pure function that takes a
  `launchctl print` output string + expected plist path, and unit-test it
  with synthetic outputs (loaded-at-correct-plist, loaded-at-stale-plist,
  not-loaded, mangled output). Codex finding #6 from the port plan.
  Effort: S (CC ~15 min).
- **`.app/Contents/Resources/AutoWhisper.icns`** — convert existing PNG
  icons via `iconutil` in `cmake/MacOSBundle.cmake`, add `CFBundleIconFile`
  to `Info.plist.in`. Cosmetic Finder icon; menu-bar tray uses SF Symbols
  so doesn't affect functionality. ISSUE-001 from the /qa report.
  Effort: S (CC ~15 min).
- **Settings UI `/api/config` PUT float precision** — round floats to their
  source precision on write so `silence_duration = 0.3` doesn't become
  `0.30000001192092896` after a round-trip. Pre-existing, not mac-specific.
  ISSUE-002 from the /qa report. Effort: S.

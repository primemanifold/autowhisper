# Onboarding Audit

Last updated: 2026-05-03

## Current known onboarding surfaces

- Linux: CLI/PPA install plus `autowhisper doctor` and user service setup in `README.md`.
- macOS: public ZIP guidance in `README.md`, app bundle, permissions, settings/model download work in progress.
- iOS: SwiftUI shell shows foreground recording and permission prompts, but no real local transcription yet.

## Friction points

- Model download and permissions are activation-critical.
- Platform-specific limitations must be visible before users hit failure.
- Windows and iOS must not look production-ready before runtime proof exists.

## Recommended next onboarding checks

1. macOS downloaded-ZIP first-run smoke test.
2. iOS interruption/background state copy.
3. Linux X11 vs Wayland warning in first-run docs.
4. Model missing/download failure UX audit.

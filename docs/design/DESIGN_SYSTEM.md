# Design System

Last updated: 2026-05-03

## Current design direction

AutoWhisper should feel warm, minimal, privacy-first, and precise. Existing settings work uses `--aw-` tokens in `src/settings/web/styles.css` and the macOS/iOS surfaces are native Swift/SwiftUI.

## Token sources

- Web settings tokens: `src/settings/web/styles.css`.
- Native macOS settings app: `platform/macos/SettingsApp.swift`.
- iOS app shell: `ios/AutoWhisperApp/ContentView.swift`.

## Required voice states

- idle
- listening / recording
- preparing recording
- processing / decoding
- inserting / copied
- failed
- permission blocked
- microphone unavailable
- offline
- low confidence

## Principles

- Do not add one-off UI without updating this file.
- Treat permission, model, microphone, and processing states as first-class UI components.
- Use native platform conventions where they materially reduce friction.
- Accessibility is a product feature, not a post-pass.

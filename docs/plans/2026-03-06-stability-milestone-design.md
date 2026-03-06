# v0.5.0 Stability Milestone — Linux Stable + Clean Architecture

Date: 2026-03-06

## Goal

Remove the GTK settings dialog (main crash source), simplify the tray to a minimal indicator, make `autowhisper config` open the TOML in $EDITOR, and clean up platform abstractions so macOS can be added later without restructuring.

## Decisions

- **Tray**: Minimal icon (state indicator) + menu with "Open Config" and "Quit". No settings dialog.
- **Settings**: `autowhisper config` opens `~/.config/autowhisper/config.toml` in $EDITOR. No GUI.
- **Wayland**: X11 only for now, rely on XWayland. Wayland is a separate future effort.
- **Scope**: Linux stable first. macOS/Windows stay as stubs but architecture is ready.
- **Sound feedback**: No changes needed (already cross-platform via miniaudio).

## Changes

### 1. Delete GTK Settings Dialog

- Remove `src/tray/platform/tray_gtk_settings.cpp` (1,144 lines)
- Remove all settings dialog launch paths from tray and CLI
- `autowhisper config` opens config file in $EDITOR (fallback: nano, then vi)
- Daemon watches config file with inotify and hot-reloads on save

### 2. Simplify Tray to Minimal Indicator

- Strip `src/tray/platform/tray_gtk.cpp` to:
  - Icon changes to show state (idle / recording / processing)
  - Menu: "Open Config", "Quit"
- GTK3 + AppIndicator still needed for icon/menu, but footprint shrinks dramatically

### 3. Clean Platform Abstractions

- Add `get_config_dir()` helper with platform-specific logic:
  - Linux: `$HOME/.config/autowhisper/`
  - macOS (future): `~/Library/Application Support/autowhisper/`
  - Windows (future): `%APPDATA%/autowhisper/`
- Add `get_runtime_dir()` helper for PID file instead of hardcoded `/tmp/`
- Ensure hotkey.h, tray.h, output.h have clean interfaces for platform stubs

### 4. Sound Feedback — No Changes

Already cross-platform via miniaudio. Keep beep-on-start, beep-on-stop.

## What's NOT in Scope

- macOS implementation (CGEventTap, NSStatusBar, etc.)
- Windows implementation
- Wayland native support
- TUI or CLI config wizard

# AutoWhisper on macOS

AutoWhisper runs natively on macOS 12+ as a menu-bar app. Hotkey capture
uses `CGEventTap`, text injection uses `CGEventPost`, and the tray uses
`NSStatusItem`. Inference runs on Metal via whisper.cpp.

## Install

### From source

```bash
git clone --recursive https://github.com/primemanifold/autowhisper.git
cd autowhisper
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
cmake --build build --target autowhisper_bundle
./platform/macos/install.sh
```

## Permissions

macOS gates the three things AutoWhisper does. The first time you launch
the daemon, you'll be prompted for each — grant them, then restart the
service.

1. **Microphone** (audio capture). Prompt appears the first time audio
   recording starts. `Info.plist` contains the usage string.
2. **Input Monitoring** (global hotkey listener). Granted via System
   Settings \u2192 Privacy & Security \u2192 Input Monitoring.
3. **Accessibility** (text injection / cmd+V). Granted via System
   Settings \u2192 Privacy & Security \u2192 Accessibility.

After granting, quit and restart the service:

```bash
autowhisper doctor          # should now show all three as OK
autowhisper restart
```

### Why three permissions?

- Reading keystrokes requires Input Monitoring.
- Posting keystrokes (typing the transcript, sending cmd+V) requires
  Accessibility / Post-event access.
- Recording audio requires Microphone access.

If only some are granted, AutoWhisper degrades gracefully:
- No Microphone \u2192 no recording, doctor flags it.
- No Input Monitoring \u2192 hotkey doesn't fire, doctor flags it.
- No Accessibility \u2192 transcript is copied to the clipboard (no auto-paste,
  no Enter), and the tray icon shows the ERROR state so you know to look.

## Service (launchd)

The installer drops a LaunchAgent at
`~/Library/LaunchAgents/us.primemanifold.autowhisper.plist`.

```bash
autowhisper start        # launchctl bootstrap
autowhisper stop         # launchctl bootout
autowhisper restart      # launchctl kickstart -k
autowhisper status       # launchctl print
autowhisper logs -f      # tail -f ~/Library/Logs/autowhisper/autowhisper.log
```

Logs: `~/Library/Logs/autowhisper/autowhisper.log` (rotated 10MB × 5).

## Config

Same TOML format as Linux. Default location is
`~/.config/autowhisper/config.toml`. On macOS you'll want:

```toml
[model]
size = "distil-small.en"
device = "metal"         # or "auto" — whisper.cpp detects Metal automatically
compute_type = "float16"

[hotkeys]
trigger = ["shift+super"]    # "super" = Command key
# If ⇧⌘ conflicts with Spotlight or Input sources, try ["ctrl+space"] or
# a Function-row key like ["f13"].
```

## Troubleshooting

### "Hotkey doesn't fire"

Most common cause: Input Monitoring permission missing. Run
`autowhisper doctor` — it uses `CGPreflightListenEventAccess()` so it
always agrees with the system's actual state.

If Input Monitoring is granted but the hotkey still doesn't fire, watch
the log: `autowhisper logs -f`. You'll see
`CGEventTap disabled by timeout; re-enabling` lines when macOS
rate-limits the tap (expected after sleep/wake).

### "Transcript doesn't appear at my cursor"

Most common cause: Accessibility permission missing. Check the tray icon
— ERROR state means the transcript was copied to the clipboard instead.
Click "Open Logs" in the menu; you'll see
`Platform inject failed; falling back to clipboard` or
`Transcript copied to clipboard (no auto-paste: …)`.

### "Permission was granted but still says denied"

You likely granted the permission to an older binary, then rebuilt. TCC
keys permissions to the signing identity. If you're using ad-hoc signing
(dev builds), rebuilds change the identity and permissions reset.

Fix options:
- Use Developer ID signing for dev builds (stable across rebuilds).
- Revoke the old permission (System Settings \u2192 Privacy & Security \u2192
  remove the entry) and re-grant to the new binary.

### "Menu-bar icon doesn't appear"

The `.app` bundle is required for a proper menu-bar presence. Running
the raw `build/autowhisper run` binary may or may not surface the tray
depending on terminal context. Use `/Applications/AutoWhisper.app` (or
run via `autowhisper start` so launchd hosts the process).

## Uninstall

```bash
./platform/macos/uninstall.sh
```

Preserves `~/.config/autowhisper/` and `~/Library/Logs/autowhisper/`.
Revoke TCC permissions manually in System Settings.

## Known limitations

- `autowhisper` is Developer ID–signed and notarized for release builds;
  dev builds use ad-hoc signing, which means TCC grants may reset across
  local rebuilds.
- The PulseAudio "mute-other-apps" feature is a no-op on mac (no
  equivalent API path yet). Tracked in `TODOS.md`.
- `CGEventPost` has no success signal from the target app — if the app
  rejects synthesized input, there's no way to tell. The clipboard
  always gets the transcript as a safety net.

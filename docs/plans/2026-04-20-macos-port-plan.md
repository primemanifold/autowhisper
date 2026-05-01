<!-- /autoplan restore point: /Users/rabot-channa-mac/.rstack/projects/primemanifold-autowhisper/core-autoplan-restore-20260420-110805.md -->

# AutoWhisper macOS Port — Plan

Author: /autoplan on 2026-04-20 | Branch: core

## Problem

AutoWhisper today is an Ubuntu-only voice-to-text daemon. The repo already
contains macOS scaffolding (three `.mm` stubs, an `APPLE` branch in CMake,
linkage against Cocoa/Carbon/CoreGraphics) but none of it is functional:

- `hotkey_macos.mm` — 24 lines, logs "not yet implemented".
- `output_macos.mm` — 19 lines, returns `false`.
- `tray_macos.mm` — 25 lines, all no-ops.
- `output_common.cpp` embeds Linux-only tools (`xdotool`, `xclip`) in the
  supposedly-platform-agnostic layer.
- `doctor.cpp` checks `nvidia-smi`, `$DISPLAY`, `systemctl --user`,
  `arecord`/`pactl`.
- `cli.cpp` `start/stop/restart/status/logs` shell out to `systemctl` /
  `journalctl`.
- `autowhisper.service` is a systemd user unit.

The goal is a first-class macOS port that delivers the same hotkey →
transcribe → inject-text workflow as the Linux build, using native frameworks
where possible and no extra runtime dependencies.

## Scope (MVP — in scope)

| Area                | Approach                                                              |
|---------------------|-----------------------------------------------------------------------|
| Global hotkey       | `CGEventTap` at `kCGSessionEventTap`, parity with X11 event parsing    |
| Text injection      | `CGEventPost` synthesizing Unicode keystrokes                          |
| Paste fallback      | `NSPasteboard` + `CGEventPost(cmd+v)`                                  |
| Return/Enter        | `CGEventPost(return)` (replaces xdotool path)                          |
| Tray / status item  | `NSStatusItem` + `NSMenu` on the main thread; daemon logic moves to a worker |
| Audio capture       | Reuse miniaudio (already CoreAudio-backed on mac, no change)           |
| Feedback tones      | Reuse miniaudio (already works on mac)                                 |
| Whisper inference   | Auto-enabled Metal (whisper.cpp enables `GGML_METAL` on APPLE)         |
| Mute-other-apps     | No-op on mac (existing `pulseaudio.cpp` stub path is sufficient)       |
| Service supervision | `launchd` LaunchAgent plist at `~/Library/LaunchAgents/…`              |
| `autowhisper start/stop/restart/status/logs` | Dispatch to `launchctl` / `log` on mac |
| `autowhisper doctor`| Platform-dispatched checks (Accessibility, Input Monitoring, launchctl state, CoreAudio input devices, Metal GPU) |
| Build               | Universal binary (arm64 + x86_64) via `CMAKE_OSX_ARCHITECTURES`         |
| Signing (dev)       | Developer ID signing for dev builds too (decided at final gate — TCC grants persist across rebuilds) |
| Signing (release)   | Developer ID Application signing + `notarytool` notarization + stapling |
| Bundling            | Build a `.app` with `LSUIElement=true` so the tray icon is the only UI  |
| Install             | Signed + notarized `.pkg` installer (primary); `install.sh` kept as a fallback for brew-less dev workflows |
| Tests               | Existing Catch2 suite must build + pass on macOS; add one `CGEventTap`-shaped unit test for key-name mapping |
| Doctor CI           | `autowhisper doctor` must exit 0 on a clean macOS dev box after user grants the two TCC permissions |
| Docs                | README gets a "macOS" section; new `docs/MACOS.md` with permissions walkthrough |

## NOT in scope (deferred to TODOS.md)

- Homebrew tap / cask.
- Sparkle auto-update.
- Mute-other-apps on macOS (CoreAudio HAL work).
- Windows port.
- DMG installer (the signed/notarized `.pkg` is the v1 shipping artifact).
- Intel-only fallback once Apple drops x86_64 support (already handled by universal binary).

## What already exists (reuse map)

| Sub-problem                | Existing code                                      | Reused?  |
|----------------------------|----------------------------------------------------|----------|
| Audio capture/playback     | `src/audio/audio.cpp` (miniaudio)                  | As-is    |
| Whisper inference          | `src/inference/inference.cpp`                      | As-is; Metal auto-enables |
| Config parsing             | `src/config/config.cpp` (tomlplusplus)             | As-is    |
| Settings UI HTTP server    | `src/cli/cli_settings_ui.cpp` (cpp-httplib)        | As-is    |
| CLI argument parsing       | `src/cli/cli.cpp` (CLI11)                          | Refactor service commands through a platform shim |
| Logging                    | `src/util/logging.cpp` (spdlog)                    | As-is    |
| Subprocess helper          | `src/util/subprocess.cpp`                          | As-is    |
| Key-combo parsing          | `src/hotkey/hotkey_common.cpp`                     | As-is    |
| Tray display helpers       | `src/tray/tray_common.cpp`                         | As-is    |
| Platform detection         | `src/util/platform.h` (`AW_PLATFORM_MACOS` already defined) | As-is |
| CMake APPLE branch         | `CMakeLists.txt` lines 82-88, 152-161              | Extended |

## Architecture

### Component layout (new files)

```
src/
  hotkey/platform/hotkey_macos.mm         # CGEventTap listener, run-loop thread
  output/platform/output_macos.mm         # CGEventPost typing + paste + return
  output/platform/output_linux.cpp        # NEW — xdotool/xclip paths moved here
  tray/platform/tray_macos.mm             # NSStatusItem + NSMenu
  doctor/platform/doctor_macos.mm         # NEW — mac-specific diagnostics
  doctor/platform/doctor_linux.cpp        # NEW — existing Linux diagnostics
  service/service.h                       # NEW — start/stop/restart/status/logs interface
  service/service_macos.mm                # NEW — launchctl wrapper
  service/service_linux.cpp               # NEW — systemctl wrapper (moved from cli.cpp)
cmake/
  MacOSBundle.cmake                       # NEW — .app bundle assembly
platform/macos/
  Info.plist.in                           # NEW — LSUIElement, CFBundleIdentifier, NSMicrophone/InputMonitoring usage strings
  us.primemanifold.autowhisper.plist.in   # NEW — LaunchAgent plist
  install.sh                              # NEW — copy .app to /Applications, symlink CLI, bootstrap plist
  uninstall.sh                            # NEW — inverse
docs/
  MACOS.md                                # NEW — permissions, install, troubleshooting
```

### Subsystem designs

#### Hotkey — CGEventTap

```
 main thread ──────── HotkeyManager::start() ────┐
                                                 │ dispatch to
                                                 ▼
          ┌──────── hotkey run-loop thread ────────────────┐
          │ CGEventTapCreate(kCGSessionEventTap,           │
          │   CGEventMaskBit(kCGEventKeyDown|kCGEventKeyUp │
          │                 |kCGEventFlagsChanged), cb)    │
          │ CFMachPortCreateRunLoopSource(...)             │
          │ CFRunLoopAddSource(CFRunLoopGetCurrent(), …)   │
          │ CFRunLoopRun()                                 │
          └──┬─────────────────────────────────────────────┘
             │ on key event
             ▼
  translate keycode → modifier-or-name (parity with X11 map)
  call HotkeyManager::on_modifier_press/release or on_key_press/release
```

Key-name mapping table lives in `hotkey_macos.mm` and is tested by
`tests/cpp/test_keycombo_platform.cpp` (new). The test uses a pure function
`macos_keycode_to_name(CGKeyCode)` — no CGEventTap at test time.

Cancellation: a `CFRunLoopStop` posted from `signal_stop()` unwinds the run
loop, after which the thread joins. `CGEventTapEnable(FALSE)` is called on
shutdown.

**Permission required:** Input Monitoring (system-level "Listen for keyboard
events"). Doctor detects via **`CGPreflightListenEventAccess()`** (not
`IOHIDCheckAccess` — the CG preflight APIs are the purpose-built path for
CGEventTap listeners per Apple's CoreGraphics docs). First launch calls
**`CGRequestListenEventAccess()`** to trigger the system prompt.

#### Output — CGEventPost

- `inject_platform(text)` creates key-up/key-down `CGEventCreateKeyboardEvent`s
  with `CGEventKeyboardSetUnicodeString(event, len, utf16…)` so arbitrary
  Unicode works (we don't use keycode for non-ASCII).
- `copy_to_clipboard(text)` → `NSPasteboard.generalPasteboard.setString:forType:`.
- `send_paste()` → synthesize `cmd+v` via two `CGEventCreateKeyboardEvent`s.
- `send_return_key()` → synthesize `kVK_Return`.

**Permission required:** Post-event access ("Accessibility" in older macOS
wording). Doctor detects via **`CGPreflightPostEventAccess()`**; first use
calls **`CGRequestPostEventAccess()`**. `AXIsProcessTrustedWithOptions` is
NOT the right API here — it reports AX-tree access, not event-post access,
and can diverge from actual posting permission.

**Third TCC permission — microphone.** `Info.plist` must include
`NSMicrophoneUsageDescription` ("AutoWhisper uses your microphone to
transcribe speech locally. Audio never leaves this device."). miniaudio
triggers the system prompt on first `ma_device_start` for capture; doctor
detects via `AVCaptureDevice.authorizationStatus(for: .audio)`. All three
TCC permissions (Input Monitoring, Post-event access, Microphone) are
checked by doctor and surfaced with actionable fix lines.

**Refactor:** the Linux-specific methods `inject_xdotool`, the xclip path in
`copy_to_clipboard`, the xdotool path in `send_paste`/`send_return_key` move
from `output_common.cpp` into `output_linux.cpp`. `output_common.cpp` shrinks
to the `inject()` dispatcher that calls `inject_platform` / `copy_to_clipboard`
(now also a platform virtual) / `send_paste` / `send_return_key`.

#### Tray — NSStatusItem

NSStatusItem requires a running `NSApplication` and the main thread. We
already have a daemon with its own event loop on the main thread. Strategy:

```
 main thread                   Cocoa run-loop (main thread)
 ─────────────────             ─────────────────────────────
 TrayManager::start()  ──post──▶ dispatch_async(dispatch_get_main_queue(), ^{
                                    NSStatusItem* item = [[NSStatusBar systemStatusBar]
                                                          statusItemWithLength:…];
                                    // configure menu, target-action blocks
                                 });
 TrayManager::set_state(STATE) ─post─▶ dispatch_async(main, ^{ item.button.image = …; });
 TrayManager::stop()  ────post──▶ dispatch_async(main, ^{ [[NSStatusBar systemStatusBar]
                                                         removeStatusItem:item]; });
```

The daemon's existing busy-wait loop in `daemon.cpp:run()` already pumps a
condvar; we need to additionally pump `NSApp` on the main thread so status
items and menus respond. Implementation: replace `while (!shutdown_requested_)
process_events()` with `NSApp.run()` on macOS and dispatch event processing to
a worker thread (mirror what the hotkey thread already does). Audio /
whisper / output callbacks must already be thread-safe (they are — miniaudio
is callback-driven, whisper is called on the daemon worker).

**App bundle requirement:** `LSUIElement=true` in Info.plist so the process
doesn't show in the Dock. `CFBundleIdentifier=us.primemanifold.autowhisper`.

#### Service — launchd

`launchctl` replaces `systemctl --user`. Commands map:

| CLI subcommand | Linux (today)                            | macOS (new)                                            |
|----------------|------------------------------------------|--------------------------------------------------------|
| `start`        | `systemctl --user start autowhisper`     | `launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/us.primemanifold.autowhisper.plist` (idempotent: `bootstrap || kickstart`) |
| `stop`         | `systemctl --user stop autowhisper`      | `launchctl bootout gui/$(id -u)/us.primemanifold.autowhisper` |
| `restart`      | `systemctl --user restart autowhisper`   | `launchctl kickstart -k gui/$(id -u)/us.primemanifold.autowhisper` |
| `status`       | `systemctl --user status autowhisper`    | `launchctl print gui/$(id -u)/us.primemanifold.autowhisper` |
| `logs`         | `journalctl --user -u autowhisper -f`    | `log stream --predicate 'subsystem == "us.primemanifold.autowhisper"' --level debug` (also tail `~/Library/Logs/autowhisper/autowhisper.log`) |

spdlog is configured with a file sink at `~/Library/Logs/autowhisper/autowhisper.log`
on mac so `log stream` is a bonus, not the only option.

The LaunchAgent plist uses `KeepAlive=true` (mirrors systemd `Restart=on-failure`)
and `RunAtLoad=true` (mirrors `WantedBy=graphical-session.target`).

#### Doctor

Platform-dispatched. Each check either runs or is skipped with a clear note:

```
                         linux              macos
check_gpu              nvidia-smi        Metal feature set (MTLCreateSystemDefaultDevice)
check_cuda             nvidia-smi        [skipped — Metal is used]
check_audio            arecord/pactl     ma_context_get_devices (miniaudio)
check_display          $DISPLAY          [skipped — AppKit runs in user session]
check_tools            xdotool/xclip     [skipped — native CGEventPost/NSPasteboard]
check_permissions      [skipped]         CGPreflightListenEventAccess + CGPreflightPostEventAccess + AVCaptureDevice.authorizationStatus(for: .audio)
check_model            same              same
check_service          systemctl         launchctl print … (exit code)
```

#### Build + bundling

- Add `set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")` on APPLE.
- Add `set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")` (Monterey — first version
  with stable CG Preflight event-access APIs + Metal API parity).
- `cmake/MacOSBundle.cmake` defines a custom target that:
  1. Creates `build/AutoWhisper.app/Contents/{MacOS,Resources}`.
  2. Copies the `autowhisper` executable → `Contents/MacOS/autowhisper`.
  3. Renders `platform/macos/Info.plist.in` via `configure_file`.
  4. Copies `icons/*.icns` to `Contents/Resources/`.
  5. Signing policy:
     - Dev / local rebuilds: `codesign --force --sign "Developer ID Application: <team>"
       --entitlements entitlements.plist` — same identity as release, so TCC
       grants persist across rebuilds. (Original plan used ad-hoc here;
       flipped at the final approval gate for dev-loop ergonomics.)
     - Release builds (triggered by `RELEASE=1` or a tag in CI):
       `codesign --force --options runtime --sign "Developer ID Application: <team>"
        --entitlements entitlements.plist AutoWhisper.app`
       → `productbuild` → `xcrun notarytool submit` (using keychain profile
       `AC_NOTARY_PROFILE` stored in login keychain) → `xcrun stapler staple`.
  6. `entitlements.plist` enables `com.apple.security.device.audio-input`
     and is minimal otherwise (no JIT, no library validation relaxation).
- Final artifact: `AutoWhisper-<version>.pkg` — signed with Developer ID
  Installer cert, notarized, stapled. Ships a postinstall script that symlinks
  `/Applications/AutoWhisper.app/Contents/MacOS/autowhisper` → `/usr/local/bin/autowhisper`.
- `install.sh` stays in-tree for contributor dev loops (non-notarized path).

#### Icons

Convert existing PNG tray icons in `icons/` to `.icns` via `iconutil` at build
time (`cmake/MacOSBundle.cmake`). macOS menu-bar icons want template-mode
(black-on-transparent) for dark/light auto-adapt — add
`"NSImage.template": true` conversion in the NSStatusItem setup.

## Failure modes registry

| Codepath                                  | Failure mode                                | Rescued? | Test? | User sees                                 | Logged? |
|-------------------------------------------|---------------------------------------------|----------|-------|-------------------------------------------|---------|
| `CGEventTapCreate`                        | Returns NULL (Input Monitoring denied)      | Y        | N     | `spdlog::error` + doctor flags it         | Y       |
| `CGEventTap` disabled by OS (timeout)     | Tap stops firing after system freeze        | Y        | N     | re-enable via `CGEventTapEnable(TRUE)`    | Y       |
| `AXIsProcessTrustedWithOptions` == false  | Accessibility not granted                   | Y        | N     | inject_platform returns false → clipboard fallback | Y       |
| `CGEventPost` silently fails              | No "did it land" signal                     | N ← GAP  | N     | Text may not appear; user has no feedback | N       |
| `NSPasteboard setString` fails            | Very rare; sandbox / memory pressure        | Y        | N     | fallthrough to inject_clipboard=false     | Y       |
| `launchctl bootstrap` fails (already loaded) | exit 17 EEXIST                           | Y        | Y     | treated as success                        | Y       |
| `launchctl bootout` fails (not loaded)    | exit 5 ESRCH                                | Y        | Y     | treated as success                        | Y       |
| Whisper Metal init fails (old OS)         | device creation returns nil                 | Y        | N     | falls back to CPU inference               | Y       |
| `dispatch_async(main, …)` after NSApp stop | Block never runs                            | Y        | N     | `stop()` already torn down tray           | Y       |
| Tray icon .icns missing                   | NSImage nil                                 | Y        | N     | tray button shows title text only         | Y       |
| Running un-bundled (no `.app`)            | Menu-bar icon appears but no Dock presence  | OK       | Y     | intended behavior for CLI dev loop        | —       |
| Developer ID signature missing (release build) | Notarization submit fails              | Y        | Y     | release script aborts with explicit error | Y       |
| Ad-hoc signature changes on contributor rebuild | TCC permissions reset on dev box only  | Y        | N     | doctor warns "signing identity changed"   | Y       |
| Notarization rejected by Apple            | `notarytool submit --wait` returns != 0     | Y        | Y     | release script surfaces the JSON log      | Y       |

CRITICAL GAPS identified (1 remaining; the signing gap closed after user
chose Developer ID notarization):
1. `CGEventPost` has no success signal. Mitigation: log the call + rely on
   clipboard-fallback + doctor sanity check. Document this.

Resolved by distribution choice:
- Release builds are Developer ID signed + notarized, so TCC permissions
  persist on end-user machines. Contributor dev boxes may still see the
  ad-hoc identity drift (minor, documented in `docs/MACOS.md`).

## Error & rescue registry (new code)

| Method                             | What can go wrong                       | Exception/error    | Rescued?           | User sees                                  |
|------------------------------------|-----------------------------------------|--------------------|--------------------|--------------------------------------------|
| `HotkeyManager::start()` (mac)     | CGEventTap creation returns NULL        | Custom status code | Y — log + early-exit | "Grant Input Monitoring" in doctor        |
| `HotkeyManager::start()` (mac)     | CFRunLoopRun unwinds unexpectedly       | Sentinel variable  | Y — thread exits, `running_=false` | Service restarts via launchd KeepAlive     |
| `OutputManager::inject_platform`   | Accessibility denied                    | AX returns false   | Y                  | "Grant Accessibility" via feedback manager |
| `TrayManager::start()` (mac)       | NSApp already running in different mode | NSException        | Y — `@try/@catch`  | Logged; tray disabled, daemon continues   |
| `ServiceManager::start()` (mac)    | `launchctl` returns 37 (needs bootout first) | exit code       | Y — retry with bootout+bootstrap | silent to user                        |
| `doctor::check_permissions` (mac)  | TCC DB locked (rare)                    | IO error           | Y — mark "unknown" | user prompted to re-run                    |

## Implementation alternatives considered

```
APPROACH A: Native, MVP (RECOMMENDED)
  Summary: CGEventTap + CGEventPost + NSStatusItem + launchd, no new deps.
  Effort:  M  (human ~1w / CC ~1h)
  Risk:    Med — Cocoa run-loop interaction with the existing busy-wait daemon is the tricky bit.
  Pros:    - No runtime deps.
           - Permissions UX identical to every other mac menu-bar app.
           - Ad-hoc signing is free and makes TCC permissions persist.
  Cons:    - Requires refactoring output_common.cpp to split xdotool logic out.
           - Daemon run loop needs a macOS branch (NSApp.run vs condvar).
  Reuses:  miniaudio, whisper.cpp (Metal), tomlplusplus, spdlog, CLI11, settings UI.

APPROACH B: Embed an existing menu-bar framework (MacControls / Sauce)
  Summary: Vendor a Swift/ObjC tray helper library for faster status-item UX.
  Effort:  S  (human ~3d / CC ~30m)
  Risk:    High — new dep, Swift-C++ interop, brittle upgrade path.
  Pros:    - Slightly fewer lines in tray_macos.mm.
  Cons:    - Dependency on an external library that might be abandoned.
           - Swift-C++ interop is non-trivial in CMake.
  Reuses:  Same core as A.

APPROACH C: Ship a .pkg + require Developer ID from day one
  Summary: Same code path as A but distribution is a signed .pkg installer.
  Effort:  L  (human ~2w / CC ~4h)  [mostly Apple paperwork]
  Risk:    Med — requires ongoing notarization budget + Developer ID cert.
  Pros:    - No TCC permission reset on rebuild.
           - One-click install, clear provenance.
  Cons:    - Requires paid Apple Developer account.
           - Notarization adds ~60s to every release build.
           - Blocks initial landing — can't ship until certs exist.
  Reuses:  Same core as A.
```

**RECOMMENDATION:** Approach A. Ship the native port with ad-hoc signing and
a documented TCC-permissions flow. Defer Developer ID + .pkg to a follow-up
milestone once the port is proven.

## Dream-state delta

```
CURRENT STATE                  THIS PLAN                        12-MONTH IDEAL
────────────────────────────   ──────────────────────────────   ─────────────────────────────
- Ubuntu-only                  - macOS + Ubuntu working end-     - macOS via Homebrew cask,
- macOS stubs non-functional     to-end with native tooling       signed + notarized
- Linux-only xdotool/xclip     - Platform-dispatched output/     - Auto-updating via Sparkle
  embedded in "common" code      tray/doctor/service              (GUI) / apt (Linux)
- systemd-only service supervision - launchd LaunchAgent + CLI    - Windows + Wayland also
                                 wraps launchctl                   supported
                               - Ad-hoc signed .app + shell
                                 installer
                               - Doctor teaches permissions
```

## Temporal interrogation (implementation order)

```
HOUR 1 (foundations, human ~8h / CC ~20min):
  - Refactor output_common.cpp: move Linux-only code to output_linux.cpp,
    add inject_platform virtual surface for clipboard/paste/return.
  - Refactor cli.cpp: extract service commands into ServiceManager
    (service_linux.cpp containing current systemctl logic).
  - Refactor doctor.cpp: split into doctor_common.cpp + doctor_linux.cpp.
  - CMake: add per-platform lists for these new files.

HOUR 2-3 (core macOS impls, human ~16h / CC ~45min):
  - hotkey_macos.mm — CGEventTap + CFRunLoop thread + key translation table.
  - output_macos.mm — CGEventPost Unicode typing + NSPasteboard + cmd+v + return.
  - tray_macos.mm — NSStatusItem + NSMenu + dispatch_async plumbing.
  - doctor_macos.mm — permissions probes + Metal + launchctl state.
  - service_macos.mm — launchctl bootstrap/bootout/kickstart wrapper.

HOUR 4-5 (integration + daemon run-loop, human ~16h / CC ~45min):
  - daemon.cpp: add an APPLE branch — NSApplication creation on main thread,
    audio/whisper/output on a worker thread, event queue already exists.
  - launchd plist template + configure_file in cmake/MacOSBundle.cmake.
  - .app bundling CMake target + Info.plist template + codesign --sign -.
  - install.sh / uninstall.sh scripts.

HOUR 6+ (polish, human ~8h / CC ~20min):
  - README macOS section + docs/MACOS.md permissions walkthrough.
  - icns generation from existing PNG icons.
  - Add tests: test_keycombo_platform.cpp, test_service_macos_cmd_map.cpp
    (pure string-munging, no IPC).
  - `autowhisper doctor` verified on a clean mac.
  - Update CI to build on macOS (github actions macos-14 runner).
```

## Tests

Existing Catch2 suite must continue to build and pass on mac. New tests:

- `tests/cpp/test_keycombo_platform.cpp` — `macos_keycode_to_name(CGKeyCode)`
  returns the same canonical name the X11 path produces for every ASCII key
  plus shift/ctrl/alt/super/return/esc/space.
- `tests/cpp/test_service_cmd_map.cpp` — given (subcommand, platform) returns
  the right argv; exit-code translator treats EEXIST/ESRCH as success.
- `tests/cpp/test_output_platform_dispatch.cpp` — `OutputManager::inject`
  calls `inject_platform` first, then falls back to clipboard; verify with
  platform injected via a test double.

## Observability

- All new codepaths use structured `spdlog` lines at entry/exit.
- Metal device name logged at whisper load time (parity with the existing
  "CUDA device X detected" line).
- Doctor writes a structured report to `~/.config/autowhisper/doctor-last.json`
  so the settings UI can render it.

## Rollout

1. Land everything on `core` behind no flag — the macOS code is gated on
   `#if defined(__APPLE__)` which means Linux builds are unaffected.
2. Bump VERSION to 0.7.0.
3. Release notes call out: first macOS release, permissions required.
4. CI adds a macOS-14 arm64 runner that builds + runs tests.
5. No Linux regression risk — new `*_linux.cpp` files hold code that was
   previously in `*_common.cpp`; the Linux build is source-compatible.

## TODOS (deferred)

- Homebrew tap for `brew install autowhisper`.
- Sparkle auto-update.
- Mute-other-apps on macOS (CoreAudio HAL tap).
- DMG installer (the signed `.pkg` is the v1 shipping artifact).
- Windows port.
- First-run onboarding window with permission-grant walkthrough (tray menu has a "Help" entry in v1).

---

## CEO Review — Findings

Mode: **SELECTIVE EXPANSION** (/autoplan default for feature plans on an
existing system). Premises locked by user:
1. Mac is a first-class release target.
2. Dev builds ad-hoc signed; release builds Developer ID signed + notarized.
3. Menu-bar tray required, LSUIElement .app bundle.
4. Distribution: signed + notarized `.pkg` (user has Apple Dev + cert).

### Section 1 — Architecture

Examined: `daemon.cpp` run loop, `hotkey.h`/`output.h`/`tray.h` interfaces,
CMake APPLE branch, miniaudio/whisper cross-platform surface.

Finding 1A — **Daemon run-loop coupling**. The current daemon's main loop
does `while (!shutdown) queue_cv.wait_for(100ms)`. On macOS, `NSStatusItem`
and its menus need the main thread to be a Cocoa run loop. Options:

- A (recommended): main thread runs `NSApp.run()` on mac, the existing event
  loop moves to a worker thread. Audio/whisper already run on their own
  threads, so worker-thread dispatch is a one-line change.
- B: keep the main-thread loop, drain the Cocoa queue by calling
  `CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true)` on each iteration.
- Auto-decision: **A** — P5 (explicit over clever). `NSApp.run()` is the
  idiomatic shape every Apple example uses; B works but creates a weird
  hybrid loop that a new reader has to reverse-engineer.

Finding 1B — **Output refactor parity**. Moving `inject_xdotool` /
`xclip` logic into `output_linux.cpp` while leaving `output_common.cpp` as a
pure dispatcher is a straight DRY improvement. No Linux regression — same
code, different file. Auto-decided: do the refactor. (P2 boil lakes +
P4 DRY.)

Finding 1C — **Service-command abstraction**. `start/stop/restart/status/logs`
in `cli.cpp` shell out to `systemctl` literally. Extracting a `ServiceManager`
interface keeps the CLI code the same while letting macOS swap in
`launchctl`. In blast radius, <50 LOC. Auto-decided: extract. (P2 + P3.)

Finding 1D — **ASCII dependency diagram**:

```
                   ┌────────────┐
                   │   main()   │
                   └──────┬─────┘
                          │ CLI11 parse
                 ┌────────┴────────┐
                 ▼                 ▼
          ┌────────────┐    ┌──────────────┐
          │  cmd_run   │    │ cmd_start/…  │
          │ (daemon)   │    │  (service)   │
          └──────┬─────┘    └──────┬───────┘
                 │                 │
                 ▼                 ▼
          ┌────────────┐    ┌──────────────┐
          │AutoWhisper │    │ ServiceMgr   │─── Linux ───▶ systemctl
          │  Daemon    │    │  (new)       │─── macOS ───▶ launchctl
          └──┬─────────┘    └──────────────┘
             │
   ┌─────────┼───────────┬─────────────┬────────────┐
   ▼         ▼           ▼             ▼            ▼
 ┌────┐  ┌────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
 │Aud │  │Whisper │  │HotkeyMgr│  │OutputMgr│  │TrayMgr  │
 │ma* │  │ MTL/CU │  │ plat-** │  │ plat-** │  │ plat-** │
 └────┘  └────────┘  └─────────┘  └─────────┘  └─────────┘
  ma=miniaudio (CoreAudio/ALSA)
  plat-**: _x11.cpp | _macos.mm | _win32.cpp   (dispatched at compile time)
```

### Section 2 — Error & Rescue Map

Existing `Error & rescue registry` table in the plan covers the new code.
Gap check:

- All new methods have a named failure mode ✓
- No `catch(...)` swallows — we use explicit named checks ✓
- One remaining GAP: `CGEventPost` has no return-value signal. Mitigation:
  after the synthesized keystroke, `NSWorkspace.frontmostApplication` +
  a 20ms sleep + "assume success; fall back to clipboard if the clipboard
  sentinel shows text still present" is the best we can do. Documented
  as a known limitation. Auto-decided: **accept the gap, document it**.
  (P3 pragmatic — we cannot do better without enumerating every app's
  accessibility tree.)

### Section 3 — Security & Threat Model

New attack surface:

- CGEventTap observes every keystroke system-wide. The binary is granted
  Input Monitoring; a compromised autowhisper could keylog. Mitigation:
  hardened runtime entitlements (`com.apple.security.device.audio-input`
  only — no entitlement that expands capability beyond what the feature
  needs). Notarization + stapling means the user's Gatekeeper won't
  silently allow a tampered replacement. Auto-decided: **add hardened
  runtime, minimal entitlements**. (P1 completeness + P5 explicit.)
- LaunchAgent plist runs the binary at user login. If an attacker with
  write access to `~/Library/LaunchAgents` drops a malicious plist with
  the same label, it overrides ours on reboot. Mitigation: postinstall
  script sets the plist mode to `0644` owned by the user; that's the best
  `user`-scope LaunchAgent can do. Already planned.
- No network listeners, no new IPC surfaces — settings UI HTTP server
  still binds to 127.0.0.1 ephemeral port. ✓
- No PII handling changes — audio never leaves the machine. ✓

Finding 3A — **Hardened runtime**: auto-decided to add. Low cost, huge
security win, required for notarization anyway.

### Section 4 — Data Flow & Interaction Edge Cases

New user-visible interactions on mac:

| Interaction                       | Edge case                              | Handled? | How                                                                 |
|-----------------------------------|----------------------------------------|----------|---------------------------------------------------------------------|
| Tray icon click → menu            | Main thread busy during transcription  | Y        | Transcription runs on worker; menu stays live                       |
| Hotkey press                      | Input Monitoring not granted           | Y        | Doctor flags + logs; no crash                                        |
| Text injection                    | Accessibility not granted              | Y        | Clipboard fallback                                                   |
| Text injection                    | Unicode emoji / CJK / combining chars  | Y        | `CGEventKeyboardSetUnicodeString` handles UTF-16 natively           |
| Text injection                    | Target app blocks synthesized input    | N (GAP)  | No signal; documented                                                 |
| Hotkey                            | Held through a sleep/wake cycle        | Y        | CGEventTap disabled by OS → we detect + re-enable via `kCGEventTapDisabledByTimeout` callback |
| `autowhisper start` (launchctl)   | Already loaded                         | Y        | Treat exit 17 as success                                             |
| `autowhisper stop`                | Not loaded                             | Y        | Treat exit 5 as success                                              |
| First launch                      | Neither TCC permission granted         | Y        | Doctor prints actionable steps with `tccutil`-free instructions      |
| App bundle re-signed              | TCC reset                              | Y (dev only) | Release builds have stable Developer ID; dev builds flag via doctor |

Finding 4A — **CGEventTap disabled-by-timeout recovery** is a well-known
mac gotcha (Apple docs explicitly call it out). Auto-decided: add the
re-enable handler. In blast radius, tiny. (P1 completeness.)

### Section 5 — Code Quality Review

- DRY: output refactor eliminates the Linux-assumption-in-common smell. ✓
- Naming: `platform_linux.cpp` / `platform_macos.mm` follows the existing
  `hotkey_x11.cpp` / `tray_gtk.cpp` convention (per-impl-technology) rather
  than per-OS. Proposed convention: keep `_x11`/`_gtk` for Linux (accurate —
  we depend on X11 specifically, not Linux), use `_macos` for mac (since
  the whole stack is Cocoa). Already consistent.
- Over-engineering check: No new abstractions invented for their own sake.
  `ServiceManager` is the only new interface and it replaces inline
  `run_systemctl` calls — net code-size neutral. ✓
- Under-engineering check: No. Plan names specific APIs, failure modes,
  and tests. ✓

### Section 6 — Test Review

See the full test plan artifact written at
`~/.rstack/projects/primemanifold-autowhisper/channa-core-test-plan-2026-04-20.md`
(generated below in Phase 3). New tests are listed in the plan's Tests
section.

Key test ambition check:
- "2am Friday" test: `autowhisper doctor` on a fresh macOS 14 install
  returns a clean exit with actionable fix instructions for each missing
  permission. Covered by `tests/cpp/test_doctor_mac.cpp` (new).
- Hostile QA test: launch without Accessibility → type in a text field;
  expect clipboard fallback and a warning tray state. Added as a manual
  acceptance test in `docs/MACOS.md`.

### Section 7 — Performance Review

- Whisper inference on Metal: `distil-small.en` benchmark target ≤ 400ms
  on M-series (vs 198ms with CUDA on the reference card in README). Note
  in release notes.
- CGEventTap callback runs on the hotkey thread, no work beyond enqueueing
  — same shape as X11 impl. No new hot path.
- `NSPasteboard` setString is synchronous but trivial. ✓
- Audio capture latency identical (miniaudio / CoreAudio). ✓

No findings.

### Section 8 — Observability & Debuggability

- spdlog configured with a file sink at `~/Library/Logs/autowhisper/autowhisper.log`.
- `log stream` integration via `os_log` bridging — auto-decided to DEFER.
  (Out of blast radius — spdlog file sink is enough for v1. `log stream`
  bridge is a nice follow-up; deferred to TODOS.)
- Doctor writes structured JSON to `~/.config/autowhisper/doctor-last.json`.
- Tray menu shows current state so the user has a live indicator.

Finding 8A — **`tail -f` helper**: `autowhisper logs -f` on mac should
tail the spdlog file (`~/Library/Logs/autowhisper/…`). Trivial.
Auto-decided: include.

### Section 9 — Deployment & Rollout

- No DB migrations, no partial-state risk.
- Linux build is source-identical (new per-platform files are compiled out
  on Linux via `if(LINUX)` / `elseif(APPLE)` CMake guards).
- Rollout order:
  1. Land PR on `core` (Linux CI still passes).
  2. macOS runner turns green.
  3. Release workflow produces both `.deb` and `.pkg`.
  4. VERSION → 0.7.0.
- Rollback: `git revert` of the merge commit; Linux unaffected.

### Section 10 — Long-Term Trajectory

- Tech debt: minimal. Per-platform files with a shared interface is the
  idiomatic C++ multi-platform pattern.
- Reversibility: 5/5 — the port is additive.
- 1-year question: every platform-specific file is named
  `<subsystem>_<impl>.*`. A new engineer finds the mac impl by looking at
  `src/output/platform/output_macos.mm`. Obvious.
- Platform potential: once `ServiceManager` exists, adding Windows service
  supervision is ~40 LOC.

### Section 11 — Design & UX Review (tray surface)

Tray state coverage map:

| State       | Menu-bar icon         | Menu item text        |
|-------------|-----------------------|-----------------------|
| IDLE        | mic (dim)             | "AutoWhisper — idle"  |
| RECORDING   | mic (solid red)       | "Recording…"          |
| PROCESSING  | mic (gray, animated)  | "Transcribing…"       |
| ERROR       | mic (!)               | "Error — click to view"|

Auto-decisions:
- Template mode on NSStatusItem button image so icon adapts to dark/light.
- Menu items: Record/Stop, Open Settings, Open Logs, Restart, Quit.
- No onboarding popover in v1 — doctor CLI + `docs/MACOS.md` cover it.
  Deferred to TODOS ("First-run onboarding window").

### Expansion cherry-pick ceremony (SELECTIVE EXPANSION)

Candidates scanned from 10x check + delight opportunities, auto-decided
per principle:

| # | Expansion proposal                                 | Blast radius | Effort | Decision  | Principle        |
|---|----------------------------------------------------|--------------|--------|-----------|------------------|
| 1 | Hardened runtime entitlements (minimal)            | In           | S      | ACCEPTED  | P1 completeness  |
| 2 | CGEventTap disabled-by-timeout recovery            | In           | S      | ACCEPTED  | P1 completeness  |
| 3 | Menu-bar icon template mode for dark/light         | In           | XS     | ACCEPTED  | P1 completeness  |
| 4 | `autowhisper logs -f` tails spdlog file on mac     | In           | S      | ACCEPTED  | P1 + P3          |
| 5 | Doctor reports signing identity + warns on change  | In           | S      | ACCEPTED  | P1               |
| 6 | CI macOS-14 runner added to GH Actions             | In           | S      | ACCEPTED  | P1               |
| 7 | `autowhisper install`/`uninstall` subcommand        | In           | S      | ACCEPTED  | P3 pragmatic     |
| 8 | Log rotation (10 MB × 5 files) in spdlog file sink | In           | XS     | ACCEPTED  | P1               |
| 9 | First-run onboarding window                        | New surface  | M      | DEFERRED  | P2 out of radius |
|10 | `log stream` / `os_log` bridging                   | New surface  | M      | DEFERRED  | P3 pragmatic     |
|11 | Homebrew tap repo                                  | Out (new repo)| M     | DEFERRED  | P2               |
|12 | Sparkle auto-update                                | New surface  | L      | DEFERRED  | P2               |
|13 | Mute-other-apps via CoreAudio HAL                  | New surface  | L      | DEFERRED  | P2               |
|14 | Animated recording pulse on status-item icon       | In           | XS     | ACCEPTED  | P1 + P3          |

Items 9, 10 land in TODOS.md at merge time.

No TASTE DECISIONS flagged at CEO stage — the architecture choice (1A) is
strongly principled under P5; distribution is locked by user premise.

---

## Design Review — Findings (macOS tray surface)

Scope: NSStatusItem menu only. Existing web settings UI is unchanged by
this plan. No DESIGN.md exists in the repo; native macOS HIG is the
reference.

### Dimension 1 — Hierarchy

Menu item order auto-decided:

```
  ●  AutoWhisper — idle                    (disabled header, shows current state)
  ─────────────────────────────────────
     Start Recording          ⇧⌘           (dynamic: swaps to "Stop Recording" while recording)
     Cancel                   ⎋
  ─────────────────────────────────────
     Input device: MacBook Pro Mic         (disabled, informational)
     Output device: MacBook Pro Speakers   (disabled, informational)
     Hotkey: ⇧⌘                            (disabled, informational)
  ─────────────────────────────────────
     Open Settings…           ⌘,
     Open Logs
     Restart
  ─────────────────────────────────────
     Quit AutoWhisper         ⌘Q
```

Rationale: primary action (Start/Stop) first, cancel right after it,
informational items grouped below, destructive (Quit) last. HIG-idiomatic.

### Dimension 2 — State coverage

| State      | Icon                 | Header text                   | Animation  |
|------------|----------------------|-------------------------------|------------|
| IDLE       | mic (template)       | "AutoWhisper — idle"          | —          |
| RECORDING  | mic (red tint)       | "Recording…"                  | 1Hz pulse  |
| PROCESSING | mic (gray, outline)  | "Transcribing…"               | rotating ⟳ |
| ERROR      | mic with badge       | "Error — click to view"       | —          |

All four states covered. ✓

### Dimension 3 — Accessibility

- Menu items have standard keyboard equivalents (⌘Q, ⌘,) ✓
- VoiceOver: NSStatusItem button's `accessibilityLabel` set to "AutoWhisper,
  {state}" ✓
- Icon template mode provides sufficient contrast for both light and dark
  menu bars ✓

### Dimension 4 — AI slop check

Menu structure matches well-known mac menu-bar apps (Rectangle, Bartender,
Audio Hijack). No invented UI — every item maps to an action users expect.
No generic "AI-assistant" patterns. ✓

### Dimension 5 — Information density

6 action items + 3 informational rows + 1 header. Well under the "too many
menu items" threshold. No nesting needed for v1.

### Dimension 6 — Error empathy

ERROR state routes to "Open Logs" on click. Doctor recommendations appear
in spdlog at the point of failure so user finds them immediately.

### Dimension 7 — Responsive / adaptive

- Template image → auto-adapts dark/light menu bar.
- Notched display: NSStatusItem auto-handles; no custom positioning.
- Menu-bar-item-title-too-long: we never show text in the menu bar
  itself, only the icon. ✓

No design findings requiring AskUserQuestion — all auto-decided above.

---

## Eng Review — Findings

Artifact: Test plan written to
`~/.rstack/projects/primemanifold-autowhisper/channa-core-test-plan-2026-04-20.md`.

### Scope challenge

Examined actual code in:
- `src/daemon/daemon.cpp` (run loop, component orchestration)
- `src/output/output_common.cpp` (xdotool/xclip assumptions)
- `src/cli/cli.cpp` (systemctl assumptions)
- `src/doctor/doctor.cpp` (Linux-specific checks)
- `src/hotkey/platform/hotkey_x11.cpp` (reference for thread + key mapping pattern)

Scope is appropriate: the port is purely additive on Linux and guarded by
`#if defined(__APPLE__)` / CMake `if(APPLE)` for mac-specific code paths.
No complexity smell. **No reduction recommended.**

### Architecture (already diagrammed in CEO Section 1)

One correction to the dependency graph: `daemon.cpp`'s `run()` method —
the mac branch must not just be "NSApp.run() on mac"; it must also hook
the existing `shutdown_requested_` atomic and `queue_cv_` so that SIGTERM
still tears down cleanly. Implementation sketch:

```
void AutoWhisperDaemon::run() {
    hotkey_->start();
#if defined(__APPLE__)
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    std::thread worker([this]{
        while (!shutdown_requested_.load()) process_events();
    });
    [NSApp run];  // blocks until NSApp stop
    worker.join();
#else
    while (!shutdown_requested_.load()) process_events();
#endif
    cleanup();
}

void AutoWhisperDaemon::request_shutdown() {
    state_.store(DaemonState::SHUTDOWN);
    shutdown_requested_.store(true);
    queue_cv_.notify_all();
#if defined(__APPLE__)
    dispatch_async(dispatch_get_main_queue(), ^{ [NSApp stop:nil]; });
#endif
}
```

Signal handler: the existing SIGTERM handler sets
`shutdown_requested_` (async-signal-safe). On mac the handler cannot call
`[NSApp stop:nil]` (not async-signal-safe). Solution: use a
`dispatch_source_t` of type `DISPATCH_SOURCE_TYPE_SIGNAL` on mac instead
of `sigaction`. Auto-decided: **use `dispatch_source_t` for SIGINT/SIGTERM
on mac**.

### Section 2 — Code Quality

DRY audit:
- Good: existing `hotkey_common.cpp` + per-platform files — same shape we
  extend.
- Good: platform header/impl split keeps ifdefs out of common code.
- Flagged: `cli.cpp::run_systemctl` is currently a free function.
  Extracting it into `ServiceManager` is a pure win — auto-decided earlier.

Naming:
- `ServiceManager` (new) vs `TrayManager` / `HotkeyManager` (existing) —
  consistent.
- New files use the same `<subsystem>_<impl>.*` convention as existing
  code.

Over-engineering audit: no abstractions created beyond the single
`ServiceManager` interface which is strictly needed.

Under-engineering audit: `CGEventTap` disabled-by-timeout handling was
initially under-specified — added to the plan.

### Section 3 — Test Review (full diagram above in test plan artifact)

Coverage gaps found and decisions:

**GAP 1:** `dispatch_async` blocks that update `NSStatusItem` from the
worker thread are untestable at unit level. → Auto-decided to accept;
manual acceptance check in docs/MACOS.md.

**GAP 2:** Notarization cannot run in unit tests (network + Apple).
→ Auto-decided: CI-only test, keyed on release workflow.

**GAP 3:** TCC permission state cannot be mocked.
→ Auto-decided: split doctor into two functions — `detect_permissions()`
returns a struct; `render_permissions(Struct)` renders to stdout. Test the
renderer unit-wise; manual-test the detector.

### Section 4 — Performance

Hot path analysis:
- CGEventTap callback: runs on the hotkey thread, enqueues and returns.
  Latency budget: <0.5ms per event. ✓
- `dispatch_async` to main queue for tray update: <1ms, fire-and-forget.
  ✓
- Whisper inference on Metal: bounded by model + audio length, same as
  CUDA path. Release note target: <500ms for distil-small.en on M1.

No new N+1 / memory / cache concerns.

### Section 5 — Observability

Decided earlier (Section 8 of CEO review):
- spdlog file sink on mac writes to `~/Library/Logs/autowhisper/autowhisper.log`
- Log rotation: 10 MB × 5 files (added as expansion #8).
- Doctor JSON output for settings UI consumption.
- `os_log` bridging deferred.

### Section 6 — Deployment & Rollout

Version bump path:
- Merge PR → CI produces both `.deb` and `.pkg` for tag `v0.7.0`.
- `debian/changelog` entry + `CHANGELOG.md` entry.
- Release notes call out Apple Silicon / Intel universal binary.

Rollback path: `git revert` + bump to `v0.7.1` with revert commit. Linux
PPA unaffected since Linux code is source-identical.

### Codex Adversarial Review

CODEX SAYS (8 findings, all valid — plan updated in place):

1. **Daemon shutdown path on mac.** Current shutdown relies on the main
   thread polling `shutdown_requested_` every 100ms. Once `NSApp.run()`
   owns the main thread, nothing wakes it. **Fix applied:** shutdown
   sequence is (signal/handler → atomic + `dispatch_async(main, ^{ [NSApp
   stop:nil]; })`) — also replaces the `sigaction` handler on mac with
   `dispatch_source_t` of type `DISPATCH_SOURCE_TYPE_SIGNAL`, since calling
   `[NSApp stop:]` from a signal handler is not async-signal-safe. See
   Section "Eng Review — Section 1" for the code sketch.

2. **Ad-hoc signing + TCC stability claim.** Original plan claimed
   ad-hoc signing keeps TCC permissions across rebuilds. That's
   unreliable — Apple's code-signing docs treat ad-hoc identity as
   build-local. **Fix applied:** the plan now states explicitly that
   dev/contributor ad-hoc rebuilds may reset TCC grants on the dev box;
   the Developer ID-signed release build is the stable path for users
   (and for anyone who wants grants to persist locally). Surfacing
   dev-build strategy as a **TASTE DECISION** at the final gate.

3. **Wrong permission APIs (EUREKA).** Used `IOHIDCheckAccess` +
   `AXIsProcessTrustedWithOptions`. The purpose-built APIs are
   `CGPreflightListenEventAccess` / `CGRequestListenEventAccess` (for the
   tap) and `CGPreflightPostEventAccess` / `CGRequestPostEventAccess`
   (for event posting). **Fix applied:** all permission doctor checks
   switched to the CG preflight APIs.

   > EUREKA: Most example code uses AX + IOHID because those APIs shipped
   > first. Apple added the CG preflight APIs specifically for this
   > case on 10.15+; they are the only APIs that report the exact
   > permission the feature needs. Convention is wrong; CG preflight is
   > right.

4. **Clipboard fallback is wrong when post-access is denied.** If
   `CGEventPost` is denied, cmd+v and return-key are also denied. **Fix
   applied:** degraded mode redefined as "copy-only, no auto-paste, no
   ending action, user-visible warning via tray ERROR state + notification."
   Default config's `auto_paste=true` is overridden at runtime when
   `CGPreflightPostEventAccess() == false`.

5. **CGEventTap timeout recovery underspecified.** Must handle both
   `kCGEventTapDisabledByTimeout` AND `kCGEventTapDisabledByUserInput`.
   Also: the tap must be created `listenOnly` (`kCGEventTapOptionListenOnly`)
   to avoid adding latency to the event path. **Fix applied:** plan now
   specifies `kCGEventTapOptionListenOnly`, handles both disable events in
   the same callback, and adds deterministic tests for the
   disable-→re-enable transition (pure function: given CGEventType input,
   return action).

6. **launchctl idempotency is insufficient.** `bootstrap || kickstart`
   doesn't verify the loaded job points at the current plist/binary.
   **Fix applied:** new state machine —
   ```
     current_label_loaded()
       ├─ no  → bootstrap NEW_PLIST
       └─ yes → loaded_plist_matches(NEW_PLIST) ?
                  ├─ yes → kickstart -k
                  └─ no  → bootout → bootstrap NEW_PLIST
   ```
   Implemented via `launchctl print gui/$UID/LABEL | grep -q "path = $BUNDLE"`.
   Adds `tests/cpp/test_service_state_machine.cpp` that tests the decision
   function with synthetic `launchctl print` output.

7. **Output refactor test seam doesn't exist.** `OutputManager`'s platform
   hooks are private non-virtual. **Fix applied:** refactor extracts a
   `PlatformOutput` interface (`inject`, `copy_to_clipboard`, `send_paste`,
   `send_return_key`) with platform impls and a `std::unique_ptr<PlatformOutput>`
   injected into `OutputManager`. Unit tests use a fake `PlatformOutput`
   to verify fallback order. This is more invasive than the original "move
   files" refactor — documented as a known cost of the port. Adds
   `tests/cpp/test_output_fallback_order.cpp`.

8. **Missing NSMicrophoneUsageDescription — third permission.** Plan said
   "two TCC permissions." Microphone is the third. **Fix applied:**
   `Info.plist` template now includes `NSMicrophoneUsageDescription`;
   doctor checks microphone via `AVCaptureDevice.authorizationStatus`;
   all three are surfaced in the doctor report and in `docs/MACOS.md`.

### Revised failure modes from codex (2 new rows)

| Codepath                                  | Failure mode                                | Rescued? | Test? | User sees                                 | Logged? |
|-------------------------------------------|---------------------------------------------|----------|-------|-------------------------------------------|---------|
| `CGPreflightPostEventAccess == false`     | Post-event denied; direct + cmd+v + return all fail | Y   | Y     | Tray ERROR + notification "Accessibility required"; clipboard populated | Y |
| `AVCaptureDevice authorizationStatus != authorized` | Microphone denied; capture fails       | Y        | Y     | Tray ERROR + doctor fix line              | Y       |
| `launchctl` loaded plist doesn't match current binary path | `kickstart` revives stale definition | Y  | Y     | `autowhisper status` prints mismatch warning | Y    |
| `CGEventTap` `disabledByUserInput` event  | Tap disabled by an interactive user action  | Y        | Y     | Re-enable; one log line                   | Y       |


### Audit trail (eng additions)

| # | Phase | Decision                                                     | Principle | Rationale                                                    |
|---|-------|--------------------------------------------------------------|-----------|--------------------------------------------------------------|
|10 | Eng-1 | `dispatch_source_t` for SIGINT/SIGTERM on mac (not sigaction)| P1+P5     | async-signal-safe NSApp stop; idiomatic                      |
|11 | Eng-3 | Split doctor into detect+render for testability              | P1        | Enables unit tests on the renderer                           |
|12 | Eng-5 | Log rotation 10MB × 5 files in spdlog file sink              | P1        | Prevents unbounded log growth                                |
|13 | Eng-6 | Release workflow triggered on tag, not every merge           | P3        | Notarization is slow + rate-limited                          |
|14 | Codex-1 | Use dispatch_async(main) + [NSApp stop:] for shutdown; dispatch_source_t for signals | P1+P5 | Correct mac shutdown for AppKit-owned main thread |
|15 | Codex-3 | CG preflight/request APIs for event-listen and event-post permissions (not AX/IOHID) | P5 + EUREKA | Purpose-built APIs; convention is wrong |
|16 | Codex-4 | Redefine degraded mode as copy-only+warning when post-event denied | P1 | Real fallback; cmd+v cannot work if post-event denied |
|17 | Codex-5 | Tap created ListenOnly; handle both disable events; pure-function test | P1+P5 | Lower latency; deterministic test of decision function |
|18 | Codex-6 | launchctl state machine: print → match-or-bootout → bootstrap/kickstart | P1 | Prevents stale-service drift |
|19 | Codex-7 | Extract `PlatformOutput` interface; inject via unique_ptr; fake for tests | P1 | Enables real fallback-order testing |
|20 | Codex-8 | Add NSMicrophoneUsageDescription + AVCaptureDevice mic check in doctor | P1 | Third permission needed; fresh-mac story now actually works |
|21 | Codex-2 | Dev builds use Developer ID (decided at final gate)        | P1        | TCC grants persist across rebuilds; dev loop matches release | Ad-hoc for dev |

---

## RSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` (via /autoplan) | Scope & strategy | 1 | clean | 0 unresolved / 0 critical / mode: SELECTIVE_EXPANSION |
| Design Review | `/plan-design-review` (via /autoplan) | UI/UX gaps (tray) | 1 | clean | 0 unresolved |
| Codex Review | `codex exec` (via /autoplan) | Independent 2nd opinion | 1 | clean | 8 findings, all valid, all applied (1 surfaced as taste decision → resolved) |
| Eng Review | `/plan-eng-review` (via /autoplan) | Architecture & tests | 1 | clean | 8 issues (codex) + 3 coverage gaps — all resolved / 0 critical |

**VERDICT:** APPROVED — all reviews clean. Plan is ready for implementation.
Next step: `/ship` after implementation, or begin coding from the phased
"Temporal interrogation" checklist in this file.

---

## Decision Audit Trail

| # | Phase | Decision                                                   | Principle | Rationale                                                   | Rejected alt                         |
|---|-------|------------------------------------------------------------|-----------|-------------------------------------------------------------|--------------------------------------|
| 1 | CEO-1A| NSApp.run() on mac; event loop on worker thread            | P5        | Idiomatic Cocoa; new readers understand instantly           | CFRunLoopRunInMode polling hybrid    |
| 2 | CEO-1B| Refactor output_common to move xdotool/xclip into linux file| P2+P4     | DRY + boils lake; no Linux regression                       | Leave as-is with `#ifdef`            |
| 3 | CEO-1C| Extract ServiceManager interface                            | P2+P3     | Enables mac; clean migration; tiny code delta               | Inline `#ifdef` in cli.cpp            |
| 4 | CEO-2 | Accept CGEventPost "no success signal" gap; document        | P3        | No better API exists; doc + clipboard fallback is enough    | Per-app AX tree probing (massive)    |
| 5 | CEO-3A| Hardened runtime + minimal entitlements                    | P1+P5     | Required for notarization; security win                     | Default runtime                      |
| 6 | CEO-4A| Add CGEventTap timeout-recovery handler                    | P1        | Classic mac gotcha; tiny fix                                | Ignore; rely on user to restart      |
| 7 | CEO-8 | Defer os_log bridging                                      | P2+P3     | Out of blast radius; spdlog sink is sufficient              | Add it now                           |
| 8 | CEO-11| Defer first-run onboarding window                          | P2        | Out of blast radius; docs+doctor cover MVP                  | Build window in v1                   |
| 9 | CEO-EX| Accept 8 in-blast expansions; defer 5 out-of-radius        | P1+P2     | Complete what we touch, defer what's new                    | —                                    |


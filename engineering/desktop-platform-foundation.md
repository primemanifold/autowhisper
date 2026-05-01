# Desktop platform foundation: macOS + Windows

AutoWhisper's production desktop implementation is still Linux/X11-first. This document captures the next honest platform gate for macOS and Windows: make each target buildable and make the settings UI report what is actually implemented before adding native behavior.

## Current slice

Branch: `primeodin/desktop-platform-foundation`

This slice adds:

- `src/platform/capabilities.*` — a small, testable platform readiness contract.
- `GET /api/platform` — settings-server diagnostics endpoint for Linux/macOS settings UI builds.
- Diagnostics UI card — shows build target and feature readiness in the settings app.
- Docker desktop smoke harness — Linux native build/test plus Windows MinGW cross-build artifact proof.
- MinGW CMake toolchain — `cmake/toolchains/mingw-w64-x86_64.cmake`.

## Capability honesty

### macOS

Buildable on this host with CommandLineTools. macOS validation is host-local because Docker on macOS runs Linux containers, not macOS containers. Current app-level macOS integrations remain placeholder/partial:

- audio capture: partial, because miniaudio compiles but release UX still needs microphone permission/onboarding.
- global hotkey: placeholder.
- text insertion: placeholder.
- menu bar: placeholder.
- launchd/service packaging: unsupported in this slice.

### Windows

Validated locally as a Dockerized MinGW cross-build that emits Windows PE executables. This is buildability/static artifact proof, not a Windows runtime end-to-end proof. Runtime execution remains deferred to a Windows host, Windows VM, or explicitly Wine-capable x86_64 runner. Current app-level Windows integrations remain placeholder/partial:

- audio capture: partial, because miniaudio can target Windows but real device smoke tests require Windows.
- global hotkey: placeholder.
- text insertion: placeholder.
- system tray: placeholder.
- settings UI server: not enabled on Windows yet; `/api/platform` is available through Linux/macOS settings UI and through compiled capability tests.
- service/startup/installer: unsupported in this slice.

## Local verification

Native macOS host:

```bash
cmake -S . -B build-audit -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build-audit --target autowhisper autowhisper_tests
ctest --test-dir build-audit --output-on-failure
python3 -m unittest tests.static.test_settings_design_assets -v
node --check src/settings/web/app.js
```

Docker Linux + Windows cross-target smoke:

```bash
./scripts/desktop_docker_smoke.sh
```

The Docker harness builds an Ubuntu-based image with Linux desktop dependencies and MinGW; it then runs:

1. Linux configure/build/test for `autowhisper` and `autowhisper_tests`.
2. Windows x86_64 MinGW configure/build for `autowhisper.exe` and `autowhisper_tests.exe`.
3. `file` verification that the Windows outputs are PE32+ x86-64 executables.

The harness intentionally does **not** run the Windows `.exe` files unless a future runner adds a proven emulator/runtime path. CMake also avoids registering cross-compiled runtime tests when `CMAKE_CROSSCOMPILING_EMULATOR` is absent, so a plain Linux/arm64 Docker host does not silently pretend to execute Windows binaries.

## Next slices

1. macOS microphone-permission onboarding and explicit `NSMicrophoneUsageDescription` packaging path.
2. macOS menu-bar app shell with settings/open/quit controls.
3. macOS hotkey implementation behind the existing `HotkeyManager` seam.
4. macOS clipboard/text insertion fallback behind `OutputManager`.
5. Windows settings UI server enablement or a native Windows diagnostics surface.
6. Windows Win32 hotkey implementation.
7. Windows clipboard/SendInput fallback.
8. Windows system-tray shell and installer/startup strategy.
9. Real Windows VM/device smoke after the Docker cross-build remains green.

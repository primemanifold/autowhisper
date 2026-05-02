# Claude Scout Report — windows

Source JSON: `/tmp/autowhisper-claude-scouts/reports/windows.rerun.json`

Model/session metadata: subtype `success`, turns `3`, cost `0.4049415999999999`.

---

## AutoWhisper Windows Readiness Scout Report

---

## 1. Current Windows Proof Levels

| Proof Level | Status | Evidence |
|---|---|---|
| **Source / cross-build** | `build_proven` | `cmake/toolchains/mingw-w64-x86_64.cmake` + `scripts/desktop_docker_smoke.sh` produce PE32+ x86-64 artifacts via MinGW in Docker |
| **PE artifact** | `build_proven` | Smoke script verifies with `file` command: PE32+ x86-64 executables are emitted |
| **Wine runtime** | `unverified` | Toolchain comment: "Docker smoke cross-builds PE artifacts but does not assume Wine is available on every container platform" — Wine path is not wired |
| **Actual Windows runtime** | `unverified` | No Windows host, VM, or CI runner documented anywhere; smoke script explicitly says "Runtime execution is intentionally left to a Windows host or Wine-capable x86_64 runner" |
| **Installer / startup** | `unverified` (planned) | `service_win32.cpp` returns hard error "not implemented yet"; no `.msi`/NSIS/WiX artifact exists |

---

## 2. Feature Readiness by Area

| Feature | Status | Key Evidence |
|---|---|---|
| **Microphone / audio capture** | `build_proven` (miniaudio linked) | `capabilities.cpp`: `audio_capture = Partial` — "miniaudio can target, but needs Windows device testing"; never executed on Windows |
| **Global hotkey** | `build_proven` (placeholder) | `src/hotkey/platform/hotkey_win32.cpp`: `start()` logs **"Windows hotkey not yet implemented"**; `SetWindowsHookEx` present only in comments |
| **System tray** | `build_proven` (placeholder) | `src/tray/platform/tray_win32.cpp`: all methods no-ops; logs **"Windows tray not yet implemented"** |
| **Clipboard / text insertion** | `build_proven` (placeholder) | `src/output/platform/output_win32.cpp`: all methods return `false`; class named `win32-placeholder` |
| **Model management** | `unverified` | No Windows-specific model path code; assumed portable but never exercised on target OS |
| **Settings UI** | `unverified` | No platform-native UI surface for Windows; no evidence of Windows GUI framework integration |
| **Service / startup** | `unsupported` | `service_win32.cpp`: all methods return explicit error "Windows service [action] is not implemented yet" |
| **Doctor / diagnostics** | `build_proven` (honest fail) | `doctor_win32.cpp`: single `FAIL` check-group — "This build proves Windows artifact generation only; runtime validation still requires a Windows host, VM, or proven Wine-capable x86_64 runner." |
| **`autowhisper doctor` output** | `build_proven` | Doctor compiles and will print the honest failure message; never been run on Windows |

---

## 3. Smallest Safe Windows Readiness Slice (No Overclaiming)

The only honest claim today is:

> **"AutoWhisper cross-compiles to a Windows PE64 executable via MinGW on Linux/Docker. No feature has been runtime-proven on Windows. Status: `build_proven`."**

The slice that avoids overclaiming:

1. **Confirm and document the cross-build command** (see §4 below) — this is already real.
2. **Document the `doctor_win32` honest-fail message** as the authoritative diagnostics output for Windows.
3. **Add a static claim-boundary test** (see §5 below) that fails if any Windows capability is promoted above `build_proven`/`Placeholder` without a corresponding runtime proof flag.
4. **Mark all five capability areas explicitly** in the platform readiness matrix as `build_proven` (audio, hotkey) or `unsupported` (service) or `unverified` (model path, settings UI).

Do **not** yet:
- Claim any feature is `ready` or `partial` (in the PRD sense) on Windows.
- Add a Wine CI step unless you can verify it runs end-to-end on a known Wine version.
- Add a Windows installer until service/startup stubs are replaced with real Win32 SCM code.

---

## 4. Commands for MinGW Cross-Build and Optional Runtime Proof

### 4a. Docker MinGW cross-build (already scripted)

```bash
# Builds Docker image and cross-compiles Linux + Windows targets.
# Verifies Windows output is a PE32+ x86-64 binary via `file`.
# Proves: artifact generation only — NOT runtime.
bash scripts/desktop_docker_smoke.sh
```

### 4b. Manual MinGW cross-build (host with mingw-w64 installed)

```bash
cmake -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel

# Verify PE artifact (Linux host):
file build-windows/autowhisper.exe
# Expected: PE32+ executable (console) x86-64, for MS Windows
```

What this proves: the source compiles to a Windows binary.  
What this does **not** prove: the binary runs, microphone works, hotkeys fire, tray appears, or any user-facing feature functions.

### 4c. Wine smoke (optional — proves basic loader, not features)

```bash
# Requires Wine ≥ 8.x on an x86_64 host; install with: apt install wine64
wine build-windows/autowhisper.exe --version 2>&1

# If Wine prints a version string: binary loads under Wine.
# If Wine crashes/errors: loader or DLL dependency issue.
# What this still does NOT prove: real Windows audio devices, hotkeys, or tray under Wine.
```

### 4d. Actual Windows runtime (gold standard — requires Windows host or VM)

```batch
REM On a Windows 10/11 x64 host or VM:
.\autowhisper.exe --doctor
REM Expect: doctor_win32 FAIL message (honest placeholder output)

REM To attempt microphone:
.\autowhisper.exe --record 5
REM Expect: either miniaudio device enumeration or a clear error — document whichever occurs.
```

---

## 5. Static Tests That Should Prevent Windows Overclaims

These test patterns should be added (or verified in `tests/cpp/test_platform_capabilities.cpp`):

### 5a. Capability state floor — Windows features must not exceed `Placeholder`/`build_proven` without a proof flag

```cpp
// In tests/cpp/test_platform_capabilities.cpp
#if defined(_WIN32)
TEST(WindowsCapabilities, NoFeatureClaimedReady) {
    auto caps = build_windows_capabilities();
    // None of these may be Ready or Partial until runtime proof exists
    EXPECT_NE(caps.global_hotkey.state, CapabilityState::Ready);
    EXPECT_NE(caps.text_insertion.state, CapabilityState::Ready);
    EXPECT_NE(caps.tray_or_menu_bar.state, CapabilityState::Ready);
    EXPECT_NE(caps.service_integration.state, CapabilityState::Ready);
    // audio_capture may be Partial only once device enumeration is confirmed
    EXPECT_NE(caps.audio_capture.state, CapabilityState::Ready);
}
#endif
```

### 5b. Doctor output must not report success on Windows

```cpp
#if defined(_WIN32)
TEST(WindowsDoctor, ReportsFailNotReady) {
    auto groups = run_doctor_checks();
    bool any_pass = std::any_of(groups.begin(), groups.end(),
        [](const CheckGroup& g){ return g.status == CheckStatus::PASS; });
    EXPECT_FALSE(any_pass) << "Windows doctor must not report PASS until runtime-proven";
}
#endif
```

### 5c. Cross-compiled binary test — PE identity check (shell, not CMake test)

```bash
# In scripts/ or CI:
file build-windows/autowhisper.exe | grep -q "PE32+ executable"
# Fail if cross-build output is missing or wrong format
```

### 5d. Docs claim-boundary test (grep-based, runnable in CI)

```bash
# Prevent any doc from claiming Windows is "ready" or "supported":
grep -rn --include="*.md" \
  -e "Windows.*ready" \
  -e "Windows.*supported" \
  -e "Windows.*works" \
  engineering/ docs/ README.md \
  | grep -v "build_proven\|unverified\|placeholder\|not yet\|deferred\|cross-build only"
# Any match is a CI failure — Windows docs must use PRD vocabulary.
```

---

## Summary

**Windows is `build_proven` only.** The MinGW cross-build pipeline is real and scripted (`scripts/desktop_docker_smoke.sh`). The PE artifact check is real. Every app-level feature — hotkey, tray, text insertion, service/startup — compiles to an explicit placeholder or `unsupported` stub. No feature has been run on Windows or Wine. The `doctor_win32` output is intentionally honest: it fails with a clear message. The smallest safe next step is to document these exact proof levels in the platform matrix and add static tests that gate any future promotion of Windows capabilities past `build_proven`.

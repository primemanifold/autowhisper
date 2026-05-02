# Claude Scout Report — linux

Source JSON: `/tmp/autowhisper-claude-scouts/reports/linux.rerun.json`

Model/session metadata: subtype `success`, turns `9`, cost `0.60613765`.

---

I now have all the material needed. Here is the full Linux readiness scout report.

---

## AutoWhisper — Linux Readiness Scout Report

---

### 1. Current Linux Proof Levels

| Layer | PRD state | Evidence | Notes |
|---|---|---|---|
| **Source build** | `build_proven` | `scripts/desktop_docker_smoke.sh` — Docker image builds, CMake/Ninja compiles `autowhisper` + `autowhisper_tests` for Linux arm64 | Build is inside Docker on macOS host. No CI log of a native Ubuntu amd64 host build. |
| **Package (.deb)** | `packaged` | `debian/` tree is complete (control, rules, postinst, prerm, postrm); `build-deb.sh` wraps `dpkg-buildpackage` | No artifact produced in this inspection; `build-deb.sh` is local-only; no automation of `dpkg -i` install test. |
| **PPA install** | `unverified` | README instructs `ppa:primemanifold/autowhisper` install path; no script validates PPA liveness, package version, or `apt-get install autowhisper` outcome | Cannot confirm PPA is live or has a current build without runtime access. |
| **Service start** | `partial` | `autowhisper.service` (user systemd unit) exists and is installed by `debian/rules`; service checks are in `doctor_linux.cpp:148–181` | No script calls `systemctl --user enable --now autowhisper` and asserts success. |
| **`autowhisper doctor`** | `partial` | `doctor_linux.cpp` fully implements GPU / audio / display / tools / service checks with colored output | `doctor` is never invoked in Docker smoke or any CI step; headless-safe subset (model, config) is also untested by script. |
| **Model management** | `partial` | `debian/postinst` creates `~/.config/autowhisper/models/`; README documents `model list` / `model download` | No script or test asserts `model list` returns expected entries or that `model download tiny.en` completes successfully. |
| **Hotkey** | `partial` | `hotkey_x11.cpp` — full XRecord async implementation, select+wake-pipe shutdown, modifier/key mapping | Requires `$DISPLAY` + XRecord extension; fails silently in Docker; Wayland unsupported; no integration test exists. |
| **Text insertion** | `partial` | `output_x11.cpp` — XTest in-process + xdotool fallback + xclip/xsel clipboard; graceful degradation chain is unit-tested | Requires live X11 display and a focused window; cannot run in headless Docker; output_fallback unit tests use stubs, not real X11. |
| **Runtime E2E** | `unverified` | No evidence of end-to-end record → transcribe → inject test on Linux in any script or test file | Docker ctest filter covers `platform|api/platform|settings|schema|config` — no transcription or hotkey-trigger path. |

---

### 2. Top 5 Linux Install/Runtime Documentation or Verification Gaps

**Gap 1 — PPA liveness is unchecked.**
The README's primary install path (`add-apt-repository ppa:primemanifold/autowhisper`) has no validation in any script. If the PPA has a stale or missing package, users hit a silent `E: Unable to locate package autowhisper` with no guidance. There is no script that runs `apt-cache policy autowhisper` or validates a `.deb` artifact against the PPA.

**Gap 2 — Service start is never smoke-tested.**
`autowhisper.service` is packaged but `systemctl --user enable --now autowhisper` is never called in any script. The `debian/postinst` only prints instructions — it does not start the service. Users who skip the quick-start and go straight to hotkey use get nothing. `doctor_linux.cpp` will report `Service is stopped` or `WARN: Service not installed` but there is no script that proves the service reaches `active` state after install.

**Gap 3 — `autowhisper doctor` is untested in CI.**
`doctor` is documented as the first command to run (`autowhisper doctor # check system requirements`) and is the single most important install-time gate. Yet it is never called in `desktop_docker_smoke.sh` or any test. The headless-safe subset — model-file presence check and config validation from `doctor.cpp:common_checks()` — could be exercised without X11, but no test does this.

**Gap 4 — Wayland users receive no warning.**
`hotkey_x11.cpp` requires `XOpenDisplay` to succeed and `XRecord` to be available. On Ubuntu 22.04+ the default session is Wayland; `DISPLAY` may be set via XWayland but XRecord is typically unavailable under XWayland, causing the hotkey listener to log `XRecord extension not available` and return. Neither the README, the `doctor` output, nor the service logs include any user-facing Wayland warning or fallback suggestion. The PRD defers Wayland parity but says nothing about notifying users of the limitation.

**Gap 5 — Model download path has no end-to-end verification.**
`postinst` creates the models directory but downloads nothing. `autowhisper model download <name>` is documented but no script proves the download URL is reachable, the file lands in the right path, or the resulting model is valid for whisper.cpp inference. If the download host changes or the model format bumps a version, users see only a runtime error at first recording attempt — not at install time.

---

### 3. Smallest Safe Linux Readiness Slice

Add one shell script — `scripts/linux_docker_validation.sh` — that extends `desktop_docker_smoke.sh` with:

1. `./build-docker-linux/autowhisper --version` — proves the compiled binary is executable and returns a version string (no X11 needed).
2. A headless `autowhisper doctor --skip-display --skip-audio` run (or equivalent env-var gate), asserting exit 0 on model + config checks only. This requires adding a `--headless` / `--skip-x11` flag to `doctor`, a one-liner change.
3. `autowhisper model list` — asserts the model registry prints expected entries (no download, no network; just the list of known models).

That trio proves: binary works, headless doctor gates pass, model catalog loads — all from Docker, no real desktop required. Each step is independently reversible and adds less than 50 lines total.

---

### 4. Commands That Prove Linux Build/Static Readiness vs. Commands Requiring Real X11 Desktop

#### Build/static readiness — provable in Docker or CI without a display

```bash
# Full Docker build + compile + unit tests (covers platform/settings/config/schema)
bash scripts/desktop_docker_smoke.sh

# Equivalent steps manually (inside Docker or Ubuntu host):
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build --target autowhisper_tests autowhisper -- -j$(nproc)
ctest --test-dir build --output-on-failure   # unit tests only; no X11

# Binary existence + version (proves linkage succeeds)
./build/autowhisper --version

# .deb package artifact (no install)
bash build-deb.sh
ls ../autowhisper_*.deb
dpkg --info ../autowhisper_*.deb
dpkg --contents ../autowhisper_*.deb | grep -E 'usr/bin|systemd|models'
```

**What these prove:** CMake build succeeds, Catch2 unit tests pass (hotkey parsing, output fallback, config validation, settings handlers, sidecar), binary links correctly, `.deb` artifact exists with expected file tree.

**What these do NOT prove:** PPA install works, service starts, `doctor` passes, hotkey captures keystrokes, text injection reaches a window, a model downloads, end-to-end transcription produces correct output.

#### Commands requiring a real X11 desktop runtime

```bash
# Must be run from a logged-in graphical X11 session (echo $DISPLAY → :1 or similar)

# Install from PPA (proves PPA is live and package installs)
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update && sudo apt install autowhisper

# Service lifecycle
systemctl --user enable --now autowhisper
systemctl --user status autowhisper       # assert: Active: active (running)
journalctl --user -u autowhisper -f       # watch for hotkey listener started log

# Doctor (requires DISPLAY, audio device, GPU optional)
autowhisper doctor                         # all checks including X11 display, xdotool, xclip

# Model download (requires network + write access to models dir)
autowhisper model list
autowhisper model download tiny.en
ls ~/.config/autowhisper/models/           # assert file exists and is non-zero size

# Hotkey smoke (requires graphical session + focused text window)
# Open a text editor, press Shift+Super, speak "hello world", release
# Assert text appears in editor → requires human/VNC interaction or xdotool-driven automation

# Text injection integration (requires $DISPLAY + focused window)
# xdotool getactivewindow  → assert non-zero window id
# then trigger a recording and assert text appears

# E2E transcription
# systemctl --user start autowhisper → press hotkey → speak → assert text in focus window
```

---

### 5. README/Usage Guide Screenshots or Captions Needed for Linux Users

The current README has **no screenshots, no GIFs, and no terminal captures**. The following seven media items are needed to make the Linux UX verifiable at a glance:

| # | File / Caption | What to show | Why it matters |
|---|---|---|---|
| 1 | `docs/media/linux-install-terminal.png` | Terminal: `add-apt-repository`, `apt update`, `apt install autowhisper` with "Setting up autowhisper..." output | Proves PPA install completes; sets expectations on output |
| 2 | `docs/media/linux-doctor-passing.png` | `autowhisper doctor` with all checks green: GPU ✓, Audio ✓, X11 display ✓, xdotool ✓, xclip ✓, Service running ✓ | The single most important new-user gate; shows what "ready" looks like |
| 3 | `docs/media/linux-doctor-failing.png` | Same command with Service stopped + Wayland DISPLAY warning — ideally as a labeled callout | Shows users what to fix and prevents panic when one check fails |
| 4 | `docs/media/linux-model-download.gif` | `autowhisper model list` then `autowhisper model download distil-small.en` with progress bar | Model setup is the second blocker after install; users need to see what progress looks like |
| 5 | `docs/media/linux-service-start.png` | `systemctl --user enable --now autowhisper` + `systemctl --user status autowhisper` showing `Active: active (running)` | Service start is in quick-start but unexplained; screenshot removes ambiguity |
| 6 | `docs/media/linux-recording-demo.gif` | Screen capture: Shift+Super pressed (status indicator appears), speech captured, text injected into gedit or terminal | The payoff shot — proves the product works end-to-end; highest value media asset |
| 7 | `docs/media/linux-wayland-warning.png` OR inline admonition in README | `> **Wayland note:** AutoWhisper requires an X11 session (or XWayland with XRecord support). Ubuntu 22.04+ users: check `echo $XDG_SESSION_TYPE`. If "wayland", log out and select "Ubuntu on Xorg" at login.` | Ubuntu 22.04+ ships Wayland by default; without this callout, most new installs silently fail |

---

### Summary

Linux is the original product core and has the most complete C++ implementation (XRecord hotkey, XTest+xdotool output, doctor, systemd service, Debian packaging). But the gap between "code exists" and "provably working" is wide: PPA liveness, service start, doctor invocation, model download, and E2E transcription have **zero automated verification**. The Docker smoke script proves build and unit tests only. The smallest safe next step is `autowhisper --version` + headless doctor subset + `model list` in Docker, plus a Wayland warning in the README.

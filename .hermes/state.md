# AutoWhisper Hermes State

Current phase: Phase 1 — Config hardening in progress
Repository: `primemanifold/autowhisper`
Local path: `/Users/odin-mac-730/src/autowhisper`
Branch: `primeodin/design-system-settings-ui`
Observed commit before this slice: `d7e22da`
Campaign prompt: Hermes Operator four-hat loop for building AutoWhisper into a category-leading agentic voice-to-text product.

## Where we are

[HAT: CEO] This is the first Hermes campaign tick for AutoWhisper. The repository had no `.hermes/state.md`, `.hermes/decisions.md`, `.hermes/roadmap.md`, `engineering/architecture.md`, or `engineering/design-system-audit.md`, so Phase 0 bootstrap is required before any feature work. The product currently appears to be an offline, local-first, Ubuntu/X11 hotkey dictation app using native C++20, whisper.cpp, miniaudio, X11 text injection, a system tray, a local browser settings UI, Debian/PPA packaging, and Catch2 tests.

## Current product hypothesis

[HAT: Marketing] AutoWhisper is an offline, local-first dictation tool for Ubuntu/X11 users who want hotkey-triggered speech-to-text inserted directly into any desktop app, optimized around local Whisper/Distil-Whisper models, optional NVIDIA/CUDA acceleration, low-latency push-to-talk ergonomics, and simple systemd/PPA distribution.

## Open hypotheses

[HAT: Research]
- [UNVERIFIED] The closest eventual competitor may be Superwhisper or Wispr Flow, but Phase 1 research must verify this against ICP, platform, workflow, privacy model, latency, and distribution.
- [UNVERIFIED] Ubuntu/X11-first may be a narrow but defensible wedge if positioned as local-first power-user dictation rather than mainstream Mac dictation.
- [UNVERIFIED] README model speed claims need a reproducible benchmark harness before marketing can use them as proof.

[HAT: Engineering]
- The current substrate has a usable C++ architecture and test suite, but design-system maturity is low.
- Documentation drift exists around `autowhisper config` vs `autowhisper config ui` and GPU/CUDA wording.
- Phase 1 competitor work should not block fixing state/documentation substrate, but feature work should wait until Phase 0 DoD is complete.

## Last run summary

No prior `.hermes/state.md` existed.

## Blocked items

- Nothing blocking Phase 0 documentation bootstrap.
- Future local verification may require initializing submodules and installing Linux build dependencies. This Mac environment may not be able to fully execute the Ubuntu/X11 build/test matrix locally.

## Run 2026-04-30T14:28:38Z
Phase: Phase 0 — Orient
Hats used: Research, Marketing, Engineering, CEO
Shipped:
- Created `.hermes/` campaign state scaffolding.
- Created `engineering/architecture.md` with architecture map and product hypothesis.
- Created `engineering/design-system-audit.md` with 1–5 maturity scores and concrete gaps.
- Created `.hermes/decisions.md`, `.hermes/roadmap.md`, and `.hermes/reviews/` placeholder.
Learned:
- The repo is a C++20 Ubuntu/X11-first offline dictation app at branch `core`, commit `2778945`.
- Build separates `autowhisper_core` from the platform/application executable.
- Runtime flow is hotkey → audio capture → silence trim → whisper.cpp transcription → X11/clipboard output.
- Settings UI is schema-driven and served from an embedded local HTTP server.
- Design-system maturity is low: roughly 1.75/5 overall.
- Docs drift exists: README says bare `autowhisper config` opens GUI, while code routes bare `config` to editor and uses `config ui` for browser UI.
- Docs drift exists: source-build CUDA defaults are OFF while README/product copy leads with GPU acceleration.
- Independent read-only review agreed with the architecture/product hypothesis and flagged the same docs/design gaps.
- Local preflight passed: `git diff --cached --check` and independent pre-commit review.
Next:
- Begin Phase 1 competitor research: sourced teardowns for Wispr Flow, Superwhisper, MacWhisper, Aqua Voice, and Whisper Memos unless evidence suggests replacements.
- Build `research/benchmark.md` with CEO-weighted comparison scores and identify the closest benchmark competitor.
- In Phase 1, research competitors and build `research/benchmark.md` with sourced claims only.
Blocked on:
- Nothing.

## Run 2026-04-30T15:36:03Z
Phase: Phase 0.2 — Design intake and first production settings slice
Hats used: Research, Marketing, Engineering, CEO
Shipped:
- Preserved Channa's uploaded AutoWhisper design concept under `design/source-concept/`.
- Created `design/README.md` and `design/design-system.md` documenting the accepted design direction, tokens, IA, accessibility requirements, and implementation stance.
- Created `engineering/design-implementation-plan.md` for incremental design-system implementation.
- Created `engineering/macos-roadmap.md` making macOS a first-class strategic target while honestly noting current placeholder status.
- Rebuilt the embedded settings UI in `src/settings/web/` around paper-light `--aw-` design tokens, intent navigation, status/dirty-state affordances, and schema-driven rendering.
- Added `tests/static/test_settings_design_assets.py` and `tests/static/mock_settings_server.py` for regression and browser smoke checks.
Learned:
- The uploaded concept is React-based for design exploration, but production should stay plain HTML/CSS/JS to fit the embedded native C++ app.
- The repo already has macOS CMake branches and `.mm` files, but hotkey, text output, and tray/menu bar implementations are placeholders.
- Independent pre-commit review caught an Advanced-pane duplicate-ID/save bug; pane-scoped IDs plus `data-config-key` synchronization fixed it.
Verification:
- Passed `python3 -m unittest tests.static.test_settings_design_assets -v`.
- Passed `node --check src/settings/web/app.js`.
- Passed `git diff --check`.
- Browser mock-server visual QA passed with no console errors; Advanced edits synced back to the primary Dictation pane.
- Independent focused re-review passed after the duplicate-ID fix.
Blocked on:
- `cmake` is not installed on this host, so C++ configure/build/ctest verification could not run here.
Next:
- Install or provide CMake for local native verification, then run configure/build/ctest.
- Implement real diagnostics-backed panels for model availability, microphone availability, platform permissions, and doctor results.
- Start macOS M0 build-truth work before claiming macOS readiness.
- Continue Phase 1 competitor research before making comparative market claims.

## Run 2026-04-30T15:44:33Z
Phase: Remote branch handoff
Hats used: Engineering
Shipped:
- Created local branch `primeodin/design-system-settings-ui` from the verified AutoWhisper work.
- Rewrote the two local unpublished commits so both author and committer are `ObliviousOdin <ObliviousOdin@users.noreply.github.com>`.
- Pushed remote branch `origin/primeodin/design-system-settings-ui` to `https://github.com/primemanifold/autowhisper`.
Verification:
- Passed `python3 -m unittest tests.static.test_settings_design_assets -v` before push.
- Passed `node --check src/settings/web/app.js` before push.
- Passed `git diff --check` before push.
- Verified `git log origin/core..HEAD` shows only ObliviousOdin author/committer identities.
Blocked on:
- Nothing for branch creation.
Next:
- Open a PR from `primeodin/design-system-settings-ui` into `core` when Channa wants review/merge flow.

## Run 2026-04-30T15:56:29Z
Phase: Phase 0.3 — Settings navigation and accessibility hardening
Hats used: Engineering, Research, CEO
Shipped:
- Used Claude Code read-only review plus Hermes subagents to identify small next improvements after the design-system slice.
- Added URL-hash deep linking for settings panes (`#pane-audio`, `#audio`, etc.) while using `history.replaceState` on nav clicks to avoid back-stack spam.
- Added `aria-current="page"` to the active settings nav item.
- Fixed checkbox descriptions so `aria-describedby` attaches to the actual checkbox input rather than the wrapper element.
- Added static regression tests for pane deep-linking/accessibility, checkbox description wiring, and IA/nav target drift.
- Added `.hermes/reviews/2026-04-30-settings-nav-a11y-precommit.md`.
Learned:
- Claude Code and Hermes subagents independently converged on navigation/accessibility as the highest-leverage low-risk next slice.
- The previous UI implementation attached descriptions correctly for text/select controls but not for boolean checkbox controls.
Verification:
- Watched new static tests fail before implementation, then pass after the fix.
- Passed `python3 -m unittest tests.static.test_settings_design_assets -v`.
- Passed `node --check src/settings/web/app.js`.
- Passed `git diff --check`.
- Browser smoke verified `/index.html#pane-audio`, `aria-current`, checkbox `aria-describedby`, and nav hash updates.
- Independent pre-commit reviewer returned PASS.
Blocked on:
- Native C++ configure/build/ctest is still blocked because `cmake` is not installed on this host.
Next:
- Add a real DOM/browser regression test for settings rendering, labels, descriptions, hash navigation, dirty state, and Advanced synchronization.
- Implement diagnostics-backed panels for model availability, microphone availability, platform permissions, and doctor results.
- Start macOS M0 build-truth gate work before claiming macOS readiness.

## Run 2026-04-30T17:03:00Z
Phase: Phase 0 — Audit baseline gate
Hats used: Research, Marketing, Engineering, CEO
Shipped:
- Installed safe local audit prerequisites via Homebrew: `cmake` 4.3.2 and `cppcheck` 2.20.0.
- Initialized vendored submodules for the audit build.
- Fixed macOS AppleClang 21 audit build compatibility for vendored spdlog/bundled fmt by defining `FMT_CONSTEVAL=` only on AppleClang 21+ macOS builds.
- Fixed two C++ test files to include `<unistd.h>` for `::getpid()`/`::getuid()` on macOS.
- Created `audit/build-warnings.md`, `audit/static-analysis.txt`, and `audit/BASELINE.md` with toolchain/dependency/test/warning baseline and known product gaps.
Learned:
- [HAT: Engineering] Phase 0 native configure/build/ctest now passes on this macOS host: 83/83 CTest tests passed.
- [HAT: Engineering] The app still builds macOS platform placeholders for hotkey/output/tray; passing build does not mean macOS product readiness.
- [HAT: Research] cppcheck completed and found style/configuration findings, including one `unknownMacro` parser/configuration finding for `AUTOWHISPER_VERSION`; no build/test failure was caused by cppcheck.
- [HAT: Marketing] README docs still need cleanup around GPU/CUDA qualification and `autowhisper config` vs `autowhisper config ui` behavior before strong product claims.
- [HAT: CEO] Phase 0 gate is satisfied after small compatibility fixes; next work should move into the next smallest high-leverage slice rather than claiming 1.0.0 readiness.
Verification:
- Passed `cmake -B build-audit -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON`.
- Passed `cmake --build build-audit -j$(sysctl -n hw.ncpu || nproc || echo 2)` after clean rebuild.
- Passed `cd build-audit && ctest --output-on-failure` — 83/83 tests.
- Completed `cppcheck --enable=all --suppress=missingIncludeSystem --std=c++20 src/` with findings recorded in `audit/static-analysis.txt`.
- Passed `python3 -m unittest tests.static.test_settings_design_assets -v` — 8/8 tests.
- Passed `node --check src/settings/web/app.js`.
- Passed `git diff --check`.
Blocked on:
- No Phase 0 gate blockers remain on this host.
Next:
- Commit Phase 0 audit baseline and compatibility fixes.
- Proceed to the next bounded phase slice: either Phase 1 sourced competitor/positioning research or an engineering hardening slice that addresses documented docs drift and cppcheck/build warnings.

## Run 2026-04-30T17:12:00Z
Phase: Phase 0 — Audit baseline pushed
Hats used: Engineering, CEO
Shipped:
- Pushed commit `eb5bddb` (`chore: add build audit baseline and static analysis report`) to `origin/primeodin/design-system-settings-ui`.
Verification:
- Confirmed commit author and committer are `ObliviousOdin <ObliviousOdin@users.noreply.github.com>`.
Blocked on:
- Nothing for Phase 0 audit baseline.
Next:
- Commit this state handoff note, push it, then continue from the next smallest phase slice in a future run.

## Run 2026-04-30T23:06:22Z
Phase: Phase 1 — Config hardening slice in progress
Hats used: Engineering, Research, CEO
Shipped:
- [HAT: Engineering] Started Phase 1 with a bounded config-validation hardening slice rather than attempting all Phase 1 at once.
- [HAT: Engineering] Added structured `ValidationIssue`/`ValidationSeverity` diagnostics, `ConfigValidator::validate(const Config&)`, and `Config::validate_all()` while preserving `Config::validate()` as the throw-on-first-error compatibility API.
- [HAT: Engineering] Added `Config::load_with_diagnostics()` so TOML unknown sections/keys are surfaced as warning issues, while `Config::load()` still throws on validation errors and logs warnings.
- [HAT: Engineering] Reserved top-level `schema_version`, added `schema::version()`, `schema::is_known_section()`, and `schema::is_known_key()` helpers.
- [HAT: Engineering] Aligned schema numeric max bounds with hard validation for `model.beam_size`, `model.num_threads`, and `audio.channels`.
- [HAT: Research] Used two read-only subagents for candidate-slice selection and config API review; both recommended config hardening as the smallest high-leverage Phase 1 start.
- [HAT: Research] Used Claude Code read-only pre-commit review when available; it returned PASS with minor non-blocking notes. A Hermes subagent review initially returned FAIL on diagnostic-load semantics; the blocker was fixed and a second read-only subagent review returned PASS.
Learned:
- [HAT: Engineering] `Config::load_with_diagnostics()` must not call `Config::validate()` internally, otherwise collected validation errors are unreachable; normal `Config::load()` owns the throw-on-error behavior.
- [HAT: Engineering] Legacy `output.append_newline` needs explicit warning suppression because it is intentionally translated to `output.ending_action` for backwards compatibility.
- [HAT: CEO] Phase 1 is not complete; this is only the first gate-oriented hardening slice.
Verification:
- RED: New config/schema tests failed before implementation for missing `validate_all`, `ConfigValidator`, `load_with_diagnostics`, schema version/helpers, and schema max bounds.
- RED: Added diagnostic-load regression failed while `load_with_diagnostics()` still threw validation errors.
- GREEN: `cmake --build build-audit -j$(sysctl -n hw.ncpu || nproc || echo 2)` passed.
- GREEN: `cd build-audit && ctest --output-on-failure` passed — 92/92 tests.
- GREEN: `python3 -m unittest tests.static.test_settings_design_assets -v` passed — 8/8 tests.
- GREEN: `node --check src/settings/web/app.js` passed.
- GREEN: `git diff --check` passed.
Blocked on:
- Nothing for this slice.
Next:
- Commit and push this Phase 1 config hardening slice to `origin/primeodin/design-system-settings-ui`.
- Next Phase 1 slice should either extend settings/JSON validation to expose structured warnings/errors in API responses, or tackle the remaining first-party build warnings (`audio.h` unused `silence_threshold_samples_`; PulseAudio stub fields) with tests where feasible.
- Product/marketing Phase 1 competitor benchmark remains incomplete and should not be claimed as done.

## Run 2026-04-30T23:10:54Z
Phase: Phase 1 — Config hardening slice pushed
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Committed and pushed `cdcfa8d` (`feat: add structured config validation diagnostics`) to `origin/primeodin/design-system-settings-ui`.
- [HAT: Engineering] Confirmed commit author and committer are `ObliviousOdin <ObliviousOdin@users.noreply.github.com>`.
Verification:
- [HAT: Engineering] Before commit, reran full local gate: `cmake --build build-audit -j$(sysctl -n hw.ncpu || nproc || echo 2) && cd build-audit && ctest --output-on-failure && cd .. && python3 -m unittest tests.static.test_settings_design_assets -v && node --check src/settings/web/app.js && git diff --check`.
- [HAT: Engineering] Full local gate passed: CTest 92/92, static UI unittest 8/8, Node syntax check passed, whitespace diff check passed.
- [HAT: CEO] Phase 1 remains in progress; only the config hardening slice is complete.
Blocked on:
- Nothing for the pushed config hardening slice.
Next:
- Continue Phase 1 with a new bounded slice: expose structured config validation diagnostics through settings/API responses, or reduce documented build warnings with tests.

## Run 2026-04-30T23:24:14Z
Phase: Phase 1 — Settings HTTP diagnostics slice
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Completed the dirty in-progress slice that exposes structured `ValidationIssue` diagnostics through the settings handlers and the `PUT /api/config` HTTP boundary. Tests for this slice were already RED in the working tree from the previous max-turns failure; finished the impl to GREEN without duplicating prior work.
- [HAT: Engineering] Added a private `JsonFieldError` in `src/settings/handlers.cpp` so type-mismatched JSON fields surface as a structured `type_error` issue carrying the offending `section.key` path while preserving the existing `errors` string for backwards compatibility.
- [HAT: Engineering] Implemented `validate_json` to attach `c.validate_all()` issues onto `ValidationResult.issues` (path/code/severity/message), keeping `ValidationResult.errors` aligned for legacy callers.
- [HAT: Engineering] Added `settings::issue_to_json` serializer (`severity`/`path`/`code`/`message`) and wired it into `register_routes` for the test HTTP server, including a `json_parse_error` issue for malformed JSON bodies and an `invalid_enum`/`out_of_range` issue array for failed validation.
Learned:
- [HAT: Engineering] The `ValidationResult.errors` legacy string list still matters for prior tests, so the new path must populate both `errors` and `issues` rather than replacing one with the other.
- [HAT: Engineering] `json_to_config` only ever throws on the first failing field, so a type-error response carries exactly one structured issue, matching the new test contract.
Verification:
- RED before impl: targeted ctest filter `validate_json|issue_to_json|api/config` failed for the four new test cases (`structured issues`, `type_error issue`, `issue_to_json serializes`, `400 response includes structured issues array`, `malformed JSON returns 400 with json_parse_error issue`).
- GREEN: `cmake --build build-audit -j$(sysctl -n hw.ncpu)` succeeded with no new warnings on touched files.
- GREEN: targeted `ctest --output-on-failure -R "validate_json|issue_to_json|api/config"` — 11/11 tests.
- GREEN: full `ctest --output-on-failure` — 97/97 tests.
- GREEN: `python3 -m unittest tests.static.test_settings_design_assets -v` — 8/8 tests.
- GREEN: `node --check src/settings/web/app.js`.
- GREEN: `git diff --check`.
Blocked on:
- Nothing for this slice.
Next:
- Surface the new structured issues in the embedded settings UI (per-field error messaging, warning vs error styling) once the HTTP contract has soaked.
- Continue Phase 1 with the remaining first-party build warnings (e.g., `audio.h` unused `silence_threshold_samples_`, PulseAudio stub fields) or with sourced competitor benchmark work.

## Run 2026-05-01T00:45:45Z
Phase: Phase 1 — Clean run hygiene after settings/config diagnostics
Hats used: Engineering, CEO
Shipped:
- Investigated the apparent dirty status after Claude Code completed the settings/config diagnostics slice.
- Confirmed the only remaining uncommitted item was generated CMake output under `build-audit/`, not source code.
- Updated `.gitignore` to ignore `build-*/` directories so audit/release build trees do not make future runs look dirty.
Verification:
- Built existing `build-audit` tree successfully with `cmake --build build-audit -j$(sysctl -n hw.ncpu || nproc || echo 2)`.
- Passed targeted settings/config CTest subset: 15/15 tests.
- Passed full native CTest suite: 97/97 tests.
- Passed `python3 -m unittest tests.static.test_settings_design_assets -v`: 8/8 tests.
- Passed `node --check src/settings/web/app.js`.
- Passed `git diff --check`.
Blocked on:
- Nothing for the clean-run hygiene fix.
Next:
- Commit and push this `.gitignore` hygiene fix, then continue Phase 1 from a clean working tree.

## Run 2026-05-01T01:05:18Z
Phase: Phase 1 — Settings UI structured validation issues
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Continued from a clean branch and selected the next bounded slice from the prior HTTP diagnostics handoff: make the embedded settings UI consume structured validation issues.
- [HAT: Engineering] Added field-level issue rendering in `src/settings/web/app.js` for structured `issues` responses, including error/warning row states, `aria-invalid` on error controls, issue text in `aria-describedby`, duplicate Advanced-pane control support, and safe dataset matching instead of interpolating issue paths into selectors.
- [HAT: Engineering] Added design-token-based error/warning styles in `src/settings/web/style.css`.
- [HAT: Engineering] Updated production `src/cli/cli_settings_ui.cpp` so the real embedded `PUT /api/config` route emits the same `issues` array contract for malformed JSON and validation failures, while keeping legacy `errors` for compatibility.
- [HAT: Engineering] Added static regression checks in `tests/static/test_settings_design_assets.py`; verified RED before implementation, then GREEN after implementation.
Learned:
- [HAT: Engineering] The earlier structured issues contract was covered by settings test helpers, but production `cli_settings_ui.cpp` still returned only `errors`; the UI slice needed that companion fix to be real in the embedded settings server.
- [HAT: Engineering] Independent review passed and noted `textContent` prevents validation-message XSS; it also suggested avoiding CSS selector interpolation for issue paths, which was addressed before final verification.
Verification:
- RED: `python3 -m unittest tests.static.test_settings_design_assets -v` failed on the new structured field-issue/static style assertions before implementation.
- GREEN: `python3 -m unittest tests.static.test_settings_design_assets -v` — 10/10 tests.
- GREEN: `node --check src/settings/web/app.js`.
- GREEN: `cmake --build build-audit -j$(sysctl -n hw.ncpu || nproc || echo 2)`.
- GREEN: targeted CTest subset `ctest --test-dir build-audit --output-on-failure -R 'validate_json|issue_to_json|api/config|settings|config'` — 20/20 tests.
- GREEN: full native CTest `ctest --test-dir build-audit --output-on-failure` — 97/97 tests.
- GREEN: `git diff --check`.
Blocked on:
- Nothing for this slice.
Next:
- Commit and push the settings UI structured validation issue slice, then continue Phase 1 with either browser/manual UI smoke coverage for the settings save error path or first-party build-warning cleanup.

## Run 2026-05-01T01:30:49Z
Phase: Phase I0 — iOS foundation contract
Hats used: Research, Engineering, CEO
Shipped:
- Created branch `primeodin/ios-foundation` from `primeodin/design-system-settings-ui` at `81d23f9`.
- Ran two read-only iOS scouts: one for product/platform architecture and one for local build/tooling reality.
- Added first-party Swift package under `ios/` with `AutoWhisperCore` and a CLI verification target `AutoWhisperCoreChecks`.
- Added iOS domain foundations for mobile defaults, small bundled model catalog, permission/capability messaging, and record/transcribe workflow seams.
- Added `ios/README.md` and `engineering/ios-implementation-plan.md` documenting the honest iOS path and Xcode gate.
- Added ADR-0006 documenting that iOS is a native foreground app product surface, not a desktop behavior clone.
Learned:
- Current machine has Swift CLI and CMake, but active developer directory is Command Line Tools only; `xcodebuild` requires full Xcode and iOS SDKs are not available.
- Existing repo has no first-party iOS target; Swift sources are only vendored `whisper.cpp` examples.
- iOS cannot support AutoWhisper's desktop global hotkey, daemon/tray, PulseAudio, or arbitrary text injection assumptions.
Verification:
- RED observed first: `swift run --package-path ios AutoWhisperCoreChecks` failed on missing iOS domain types before implementation.
- GREEN passed: `swift run --package-path ios AutoWhisperCoreChecks` prints `AutoWhisperCoreChecks passed`.
- Independent review found no blockers and flagged `RecordingSession` concurrency/error-state issues; fixed by making `RecordingSession` an actor and adding a `.failed` state regression check.
Blocked on:
- Runnable iOS simulator/device app build is blocked until full Xcode is installed/selected and `iphoneos`/`iphonesimulator` SDKs plus `simctl` are available.
Next:
- Install/select full Xcode, then add SwiftUI app shell with `NSMicrophoneUsageDescription` and simulator/device build gates.
- After app shell, bind AVFoundation recording and `whisper.cpp` inference behind the Swift package seams.


## Run 2026-05-01T02:30:00Z
Phase: Desktop platform foundation — macOS/Windows build truth
Hats used: Research, Engineering, CEO
Shipped:
- [HAT: Engineering] Created branch `primeodin/desktop-platform-foundation` from the verified iOS foundation branch.
- [HAT: Engineering] Added `src/platform/capabilities.*` with stable platform capability JSON for Linux/macOS/Windows/current builds, using honest `ready`/`partial`/`placeholder`/`unsupported` states.
- [HAT: Engineering] Added `GET /api/platform` to the settings server and surfaced a Desktop platform readiness card in the Diagnostics settings pane.
- [HAT: Engineering] Added static/UI and C++ regression tests for the platform diagnostics contract.
- [HAT: Engineering] Added Docker desktop smoke infrastructure: `docker/desktop-ci/Dockerfile`, `scripts/desktop_docker_smoke.sh`, and `cmake/toolchains/mingw-w64-x86_64.cmake`.
- [HAT: Engineering] Added `engineering/desktop-platform-foundation.md` documenting macOS host-local validation, Docker Linux validation, Windows MinGW cross-build proof, and the remaining Windows runtime gate.
- [HAT: Engineering] Fixed Windows-portability issues found during cross-build work: WinSock link libs, POSIX settings-server guard for Windows, portable sidecar paths, and 64-bit sidecar hash formatting without LLP64 truncation.
- [HAT: Engineering] Improved macOS settings UI browser launch to use `open` instead of `xdg-open`.
- [HAT: Research] Captured browser screenshot evidence for Dictation behavior, Model & performance, and Diagnostics/Desktop platform readiness settings screens.
Learned:
- [HAT: Engineering] Docker on this macOS host cannot validate macOS runtime behavior; macOS proof must be host-local.
- [HAT: Engineering] Docker/MinGW can build Windows PE32+ x86-64 `autowhisper.exe` and `autowhisper_tests.exe`, but Windows runtime execution remains unproven until a Windows host/VM or Wine-capable runner is added.
- [HAT: CEO] The right 1.0 path is to make platform incompleteness visible and testable instead of pretending placeholders are product-ready.
Verification:
- GREEN native macOS host preflight: `cmake -S . -B build-audit -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON`.
- GREEN native macOS build: `cmake --build build-audit --target autowhisper autowhisper_tests`.
- GREEN native CTest: `ctest --test-dir build-audit --output-on-failure` — 101/101 tests passed.
- GREEN static UI tests: `python3 -m unittest tests.static.test_settings_design_assets -v` — 11/11 tests passed.
- GREEN JS syntax: `node --check src/settings/web/app.js`.
- GREEN whitespace check: `git diff --check`.
- GREEN Docker smoke: Linux focused Docker tests passed 31/31; Windows MinGW cross-build produced PE32+ x86-64 `autowhisper.exe` and `autowhisper_tests.exe` artifacts; runtime execution intentionally deferred.
- Independent read-only review found four blockers; fixed docs overclaiming, cross-compiled CTest registration, 64-bit hash formatting, and Windows settings-server caveat. Also fixed macOS browser launcher recommendation.
Blocked on:
- True Windows runtime/end-to-end validation requires a Windows host/VM or a proven Wine-capable x86_64 runner.
- Full native macOS product readiness remains blocked on real menu bar, permission onboarding, hotkey, insertion, and packaging slices.
Next:
- Commit and push this desktop-platform foundation slice to `origin/primeodin/desktop-platform-foundation`.
- Next engineering slice should implement one real native macOS integration gate, likely microphone permission/onboarding or menu-bar app shell, before expanding feature claims.


## Run 2026-05-01T11:25:28Z
Phase: PR landing and desktop branch reconciliation
Hats used: Engineering, CEO
Shipped:
- Merged GitHub PR #5 (`macos-port`) into `core` with squash merge after GitHub build check passed.
- Reconciled `primeodin/desktop-platform-foundation` with the updated `origin/core` that now includes the native macOS port.
- Resolved merge conflicts in `CMakeLists.txt`, `src/cli/cli_settings_ui.cpp`, and `tests/cpp/test_sidecar.cpp` by preserving both platform diagnostics and macOS platform abstractions.
- Fixed Windows cross-build regressions exposed by the merged macOS abstractions:
  - Replaced stale `OutputManager::Impl` Windows output placeholder with a `PlatformOutput` placeholder.
  - Added explicit Windows placeholder implementations for `make_service_manager()` and `platform_checks()`.
  - Wired Windows doctor/service stubs into CMake.
Verification:
- Passed native macOS build target `autowhisper autowhisper_tests`.
- Passed full native CTest: `117/117`.
- Passed static settings UI tests: `11/11`.
- Passed `node --check src/settings/web/app.js`.
- Passed `git diff --check`.
- Passed Docker desktop smoke after reconciliation: Linux focused tests `31/31`; Windows MinGW cross-build produced `autowhisper.exe` and `autowhisper_tests.exe` PE32+ artifacts.
Learned:
- The macOS PR's platform abstractions moved output/service/doctor seams; Windows placeholders must implement those new interfaces to keep cross-build proof honest.
- Docker cross-build remains valuable because it caught Windows regressions that native macOS CTest cannot see.
Blocked on:
- Windows runtime E2E remains gated on a real Windows host, VM, or proven Wine-capable x86_64 runner.
Next:
- Push the reconciled desktop branch, open/merge its PR into `core`, then sync local `core` cleanly.


## Run 2026-05-01T11:39:38Z
Phase: PR landing complete
Hats used: Engineering, CEO
Shipped:
- Merged PR #5 (`macos-port`) into `core` via squash merge.
- Reconciled `primeodin/desktop-platform-foundation` with updated `core`, fixed Windows cross-build regressions from the platform abstraction merge, and pushed branch commit `95ee3d5a7f11c850556ce9cfdb0d1a54a4d03031`.
- Opened PR #6 (`feat(desktop): add platform readiness foundation`) and merged it into `core` via squash after hosted GitHub `build` passed.
Verification before merge:
- Native macOS CTest: `117/117` passed.
- Static settings UI tests: `11/11` passed.
- `node --check src/settings/web/app.js` passed.
- `git diff --check` passed.
- Docker desktop smoke passed: Linux focused tests `31/31`; Windows MinGW cross-build produced PE32+ `autowhisper.exe` and `autowhisper_tests.exe`.
- Independent review found Windows doctor readiness fail-open; fixed by making the Windows runtime-not-implemented diagnostic a `FAIL`, not a `WARN`.
Remote state:
- `origin/core` is at `7a3b3c1 feat(desktop): add platform readiness foundation (#6)`.
- No open GitHub PRs were left after #6 merged.
Local cleanup:
- Local `core` was reset to `origin/core` after GitHub squash merge to avoid preserving pre-squash divergent branch history.
- Generated build directories were removed.
Remaining boundary:
- Windows runtime E2E is still not claimed; it requires a Windows host, VM, or proven Wine-capable x86_64 runner.
Next:
- Start the next focused production slice from clean `core` (recommended: macOS permission/onboarding or menu-bar polish gate).

## 2026-05-01 12:59 UTC — macOS app bundle gate built and verified

[HAT: Engineering] On branch `primeodin/macos-app-bundle-gate`, made the existing `autowhisper_bundle` target part of the default macOS build so a normal `cmake --build <build-dir>` emits `AutoWhisper.app`. The bundle target now ad-hoc signs by default and verifies the signature fail-closed with `codesign --verify --deep --strict`.

[HAT: Engineering] Added `NSAppleEventsUsageDescription` alongside the existing microphone purpose string, plus `scripts/macos_app_smoke.sh` and `tests/static/test_macos_app_bundle_assets.py` to keep the bundle contract covered. Fixed the stale sidecar fallback test to match the portable temp-directory contract on macOS instead of assuming literal `/tmp`.

[HAT: Engineering] Local verification passed: `cmake --build build-macos-app-gate`, `BUILD_DIR="$PWD/build-macos-app-gate" scripts/macos_app_smoke.sh`, `ctest --test-dir build-macos-app-gate --output-on-failure` (117/117), `python3 -m unittest tests.static.test_settings_design_assets tests.static.test_macos_app_bundle_assets -v` (16/16), `node --check src/settings/web/app.js`, and `git diff --check`. App proof: `build-macos-app-gate/AutoWhisper.app/Contents/MacOS/autowhisper` is a universal Mach-O binary (`x86_64` + `arm64`), `Info.plist` has bundle id `us.primemanifold.autowhisper`, `LSUIElement=true`, microphone and Apple Events purpose strings, and `codesign --verify --deep --strict` passes.

[HAT: Engineering] Additional signing check after user clarified development signing access: the active keychains expose a valid `Apple Development: [REDACTED]` identity, and `build-macos-app-devsign` successfully produced an Apple Development-signed `AutoWhisper.app` with hardened runtime. `codesign --display --verbose=4` reported authority chain `Apple Development` → `Apple Worldwide Developer Relations Certification Authority` → `Apple Root CA`, team id `[REDACTED]`, and `codesign --verify --deep --strict` passed via the smoke script. `spctl --assess` still rejects this local development build, which is expected without a notarized Developer ID release signature.

[HAT: CEO] Proof boundary: this establishes host-local macOS `.app` bundle build/smoke proof and Apple Development signing proof. It still does not establish Developer ID Application distribution signing, notarization, DMG/PKG distribution, permission-onboarding UX, LaunchAgent install smoke, or real end-to-end hotkey→record→transcribe→insert runtime proof. At that point the active keychains showed Apple Development signing identity only; `Developer ID Application` private-key identity was not visible yet.

## Run 2026-05-01T14:35:50Z
Phase: macOS Developer ID signing proof
Hats used: Engineering
Shipped:
- Verified a valid `Developer ID Application` code-signing identity is now visible in the active macOS keychains. Certificate fingerprints and identity details are treated as credentials and redacted from summaries.
- Built `AutoWhisper.app` in `build-macos-app-devid` with `-DAUTOWHISPER_SIGN_IDENTITY` set to the local Developer ID Application identity.
- Verified `codesign --display --verbose=4` reports hardened runtime and the Developer ID Application authority chain.
- Updated the macOS app smoke script wording so `spctl` evidence covers ad-hoc, Apple Development, and unnotarized Developer ID builds.
Learned:
- `codesign --verify --deep --strict` passes for the Developer ID-signed app.
- `spctl --assess --type execute --verbose=4 build-macos-app-devid/AutoWhisper.app` rejects with `source=Unnotarized Developer ID`; this proves Developer ID signing exists but notarization has not been completed yet.
Verification:
- `security find-identity -v -p codesigning` shows one valid Developer ID Application identity, redacted.
- `cmake -S . -B build-macos-app-devid ... -DAUTOWHISPER_SIGN_IDENTITY=<redacted Developer ID Application identity>` passed.
- `cmake --build build-macos-app-devid` passed and emitted `build-macos-app-devid/AutoWhisper.app`.
- `BUILD_DIR=$PWD/build-macos-app-devid scripts/macos_app_smoke.sh` passed.
Blocked on:
- Full Gatekeeper distribution acceptance remains blocked until the Developer ID-signed app is notarized and stapled.
Next:
- Add a notarization release gate using `xcrun notarytool`, then staple and re-run `spctl` to prove Gatekeeper acceptance.

## Run 2026-05-01T16:30:00Z
Phase: macOS 0.7.0 release candidate /ship
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Inspected Channa's local changes after the Developer ID proof and preserved the intended fixes: copy `config.toml` into `AutoWhisper.app/Contents/Resources`, search that bundled config after user config on macOS, timestamp Developer ID signatures, and make Finder/LaunchServices `.app` launch synthesize the foreground `run` path instead of falling through to bare CLI help.
- [HAT: Engineering] Hardened `.app` launch detection to tolerate LaunchServices `-psn_*` arguments and to terminate synthesized argv with a null pointer.
- [HAT: Engineering] Added/updated static bundle regression coverage, added a top-level `CHANGELOG.md` for the v0.7.0 release, updated the Debian changelog entry, and corrected the release workflow changelog link to the repository's `core` default branch.
- [HAT: Engineering] Rebuilt the Developer ID-signed `build-macos-app-devid/AutoWhisper.app`, submitted it to Apple notarization, stapled the ticket, and proved Gatekeeper accepts it as `source=Notarized Developer ID`.
Learned:
- [HAT: Engineering] A double-clicked app needs a bundled default config fallback because the process working directory is not the repository root.
- [HAT: Engineering] Finder/LaunchServices may pass `-psn_*` arguments, so macOS app-launch detection must not depend on `argc == 1` only.
- [HAT: Engineering] The repo's default branch is `core`; user-facing "main" release language maps to shipping into `core` unless the repository default branch changes.
Verification:
- GREEN: `cmake --build build-macos-app-gate -j$(sysctl -n hw.ncpu || echo 2)`.
- GREEN: `BUILD_DIR=$PWD/build-macos-app-gate scripts/macos_app_smoke.sh`.
- GREEN: `ctest --test-dir build-macos-app-gate --output-on-failure` — 117/117 tests.
- GREEN: `cmake -S . -B build-macos-app-devid ... -DAUTOWHISPER_SIGN_IDENTITY=<redacted Developer ID Application identity>`.
- GREEN: `cmake --build build-macos-app-devid -j$(sysctl -n hw.ncpu || echo 2)`.
- GREEN: `BUILD_DIR=$PWD/build-macos-app-devid scripts/macos_app_smoke.sh`.
- GREEN: app-run config proof from `/tmp` with empty `HOME` returned `build-macos-app-devid/AutoWhisper.app/Contents/Resources/config.toml`.
- GREEN: `python3 -m unittest tests.static.test_settings_design_assets tests.static.test_macos_app_bundle_assets -v` — 18/18 tests.
- GREEN: `node --check src/settings/web/app.js`.
- GREEN: `git diff --check`.
- GREEN: `xcrun notarytool submit ... --wait` returned `Accepted`; `xcrun stapler staple` and `xcrun stapler validate` passed; `spctl --assess --type execute --verbose=4` returned accepted with `source=Notarized Developer ID`.
- Independent read-only review initially failed on missing `-psn_*` handling; the blocker was fixed and covered in static tests.
Blocked on:
- Nothing for the v0.7.0 macOS app bundle release candidate.
Next:
- Commit/amend this release candidate, push `primeodin/macos-app-bundle-gate`, open/merge a PR into `core`, tag `v0.7.0`, upload the notarized macOS ZIP release asset, and monitor hosted release validation.

## Run 2026-05-01T17:00:00Z
Phase: v0.7.0 release infrastructure follow-up
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Merged the macOS app-bundle release candidate into `core` and force-corrected the `core` tip/tag to the verified local commit so the release commit preserves the required `ObliviousOdin <ObliviousOdin@users.noreply.github.com>` author and committer identity.
- [HAT: Engineering] Published GitHub Release `v0.7.0` with the notarized macOS ZIP and SHA-256 checksum asset.
- [HAT: Engineering] Fixed the release-event Debian binary package workflow by installing `build-essential`, matching the PPA workflow's dependency set.
Learned:
- [HAT: Engineering] GitHub squash merge rewrote the commit author/committer identity; for this repo's authoring policy, the protected release path should preserve or explicitly push the already-reviewed commit rather than relying on squash metadata.
- [HAT: Engineering] The PPA workflow built and signed the source package but failed at Launchpad FTP upload with a GitHub-hosted runner network error; the v0.7.0 GitHub release notes document this infrastructure blocker.
Verification:
- GREEN: PR #7 hosted CI passed before merge.
- GREEN: `core` push CI passed after landing the release commit.
- GREEN: `v0.7.0` GitHub Release exists with `AutoWhisper-macOS-v0.7.0.zip` and `AutoWhisper-macOS-v0.7.0.zip.sha256` assets.
Blocked on:
- Launchpad/PPA publication for `v0.7.0` is blocked by remote FTP connectivity from the GitHub runner, not by local package build/signing evidence.
Next:
- Rerun the PPA workflow or manually upload the signed source package when Launchpad/GitHub runner connectivity is healthy.

## Run 2026-05-01T17:12:00Z
Phase: v0.7.0 release artifact workflow follow-up
Hats used: Engineering
Shipped:
- [HAT: Engineering] Fixed release-event Debian artifact upload by copying built `.deb` files into `$RUNNER_TEMP/debian-package` before `actions/upload-artifact`, because upload-artifact v4 rejects `../` paths.
Learned:
- [HAT: Engineering] GitHub's upload-artifact v4 does not allow relative parent-directory patterns like `../autowhisper_*.deb`.
Verification:
- Pending hosted rerun after retag/release recreation.
Blocked on:
- Launchpad/PPA upload remains subject to the previously observed FTP connectivity issue.
Next:
- Retag/recreate the v0.7.0 release and verify the release-event CI passes.

## Run 2026-05-01T17:20:00Z
Phase: v0.7.0 native settings helper follow-up
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Added a native SwiftUI `AutoWhisperSettings` helper inside the macOS app bundle and changed the menu-bar Settings action to launch it instead of the embedded browser UI.
- [HAT: Engineering] Bundled/signs the helper alongside the main app and added static regression coverage for the native-settings path.
- [HAT: CEO] Treat this as a release-polish follow-up for the v0.7.0 macOS `.app` artifact, not a broad settings-system rewrite.
Verification:
- Passed `python3 -m unittest tests.static.test_macos_app_bundle_assets -v`.
- Passed `swiftc -typecheck -framework SwiftUI -framework AppKit platform/macos/SettingsApp.swift`.
- Passed `cmake --build build-macos-app-gate -j$(sysctl -n hw.ncpu || echo 2)` with `TMPDIR=/tmp/autowhisper-build-tmp`.
Blocked on:
- PPA upload still fails from GitHub-hosted runner with `[Errno 101] Network is unreachable` reaching Launchpad FTP; the release asset/tag path is separate and healthy.
Next:
- Run full local gates, rebuild Developer ID artifact with helper included, notarize/staple/assess, update the v0.7.0 release asset, and verify hosted CI.

## Run 2026-05-01T17:25:00Z
Phase: native settings helper review fix
Hats used: Engineering
Shipped:
- [HAT: Engineering] Fixed pre-commit review blockers before shipping the native settings helper: `swiftc` now receives the configured macOS deployment target, and the SwiftUI view avoids macOS 13-only `Grid` so the helper remains compatible with the app's macOS 12.0 target.
- [HAT: Engineering] Hardened hotkey array TOML serialization by stripping single quotes from comma-separated tokens before writing.
Verification:
- Passed `swiftc -typecheck -target arm64-apple-macos12.0 -framework SwiftUI -framework AppKit platform/macos/SettingsApp.swift`.
- Rebuilt default app bundle; `otool -l` showed `AutoWhisperSettings` helper `minos 12.0`.
- Passed `python3 -m unittest tests.static.test_macos_app_bundle_assets -v` after adding static coverage for the Swift target flag and macOS 13-only `Grid` exclusion.
- Passed `git diff --check`.
Next:
- Run full local gates, commit/push, update tag/release asset, and verify hosted checks.

## Run 2026-05-01T17:38:00Z
Phase: v0.7.0 shipped
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Landed macOS app-bundle release work on `core` through PR #7 and follow-up commits.
- [HAT: Engineering] Published GitHub release `v0.7.0` with `AutoWhisper-macOS-v0.7.0.zip` and checksum asset.
- [HAT: Engineering] Rebuilt the Developer ID artifact after adding the native SwiftUI settings helper, notarized it, stapled it, validated the staple, and confirmed Gatekeeper accepted it as `source=Notarized Developer ID`.
- [HAT: Engineering] Verified the bundled `AutoWhisperSettings` helper is built with macOS `minos 12.0`.
Verification:
- Local full gate passed: default macOS bundle build, app smoke, `ctest` 117/117, Python static tests 20/20, `node --check`, Swift type-check for macOS 12 target, and `git diff --check`.
- Hosted GitHub CI on `core` for commit `2578e61` passed.
- Hosted release-event CI for the recreated `v0.7.0` release passed.
- PPA workflow builds/signs but Launchpad upload fails from GitHub-hosted runner with network unreachable to `ppa.launchpad.net`; this is recorded as release infrastructure follow-up, not a macOS artifact blocker.
Boundaries:
- macOS artifact proof covers Developer ID signing, Apple notarization acceptance, stapling, staple validation, and Gatekeeper acceptance for the zipped `.app` release asset.
- This is still not a final DMG/PKG installer, auto-update, or full hotkey→record→transcribe→insert product acceptance pass.
Next:
- Fix or rerun Launchpad/PPA upload outside the blocked FTP path.
- Add installer UX and end-to-end macOS dictation acceptance testing in the next release slice.

## Run 2026-05-01T17:42:00Z
Phase: macOS hotkey teardown follow-up
Hats used: Engineering
Shipped:
- [HAT: Engineering] Included macOS CGEventTap cleanup hardening: the listener thread now releases its run-loop source/event tap instead of relying on main-thread teardown after join.
Verification:
- Static regression coverage exists for listener-thread event tap release in `tests/static/test_macos_app_bundle_assets.py`.
- This change was present in the worktree for the prior local build/notarized release artifact; committing it aligns source history/tag with the shipped binary.
Next:
- Push, retag, recreate release event, and verify hosted CI.

## Run 2026-05-01T18:46:44Z
Phase: Website/GitHub Pages setup
Hats used: Engineering, Marketing, CEO
Shipped:
- [HAT: Engineering] Added a simple framework-free static landing site under `site/` for GitHub Pages.
- [HAT: Engineering] Added `.github/workflows/pages.yml` using GitHub Actions Pages deployment from `core` and guarded manual deploys to `refs/heads/core`.
- [HAT: Engineering] Added static regression coverage in `tests/static/test_website_assets.py` and wired static asset tests into CI.
- [HAT: Marketing] Landing page copy focuses on offline/local-first dictation, macOS release download, Linux PPA install, and privacy posture without adding analytics or a frontend runtime.
Verification:
- RED: New website static tests failed before `site/` and the Pages workflow existed.
- GREEN: `python3 -m unittest discover tests/static -v` passed — 25/25 tests.
- GREEN: `node --check src/settings/web/app.js` passed.
- GREEN: Parsed `site/index.html` with Python `HTMLParser`.
- GREEN: Parsed `.github/workflows/ci.yml` and `.github/workflows/pages.yml` with Ruby YAML.
- GREEN: `git diff --check` passed.
- Browser visual smoke on local `python3 -m http.server` found the landing page coherent with no obvious desktop layout defects.
- Independent read-only review found the workflow structurally valid and static/privacy-safe; it flagged that repo/release links are private until the repo is made public or artifacts are mirrored.
Boundaries:
- GitHub Pages deployment will occur only after this branch lands on `core` and Pages is enabled for GitHub Actions.
- Because `primemanifold/autowhisper` is currently private, public visitors cannot access the linked GitHub repo/release assets unless repo visibility changes or downloads are mirrored elsewhere.
Next:
- Push branch `primeodin/github-pages-site`, open PR to `core`, verify CI, then decide whether to merge/deploy now or wait for public-release visibility policy.

## Run 2026-05-01T18:57:32Z
Phase: Website/GitHub Pages deployment guard follow-up
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Landed the static Pages site on `core` as commit `892e77f`, preserving the local ObliviousOdin author/committer identity.
- [HAT: Engineering] Observed the first Pages deploy fail because GitHub Pages is not supported for the current private repository plan.
- [HAT: Engineering] Updated the Pages workflow to skip deployment while the repository is private and to use `configure-pages` `enablement: true` once the repository is public.
Verification:
- GitHub API Pages enablement returned: current plan does not support GitHub Pages for this private repository.
- Follow-up static tests cover the private-repo guard and Pages enablement flag.
Boundaries:
- The static site is in `core`, but GitHub Pages will not publish while the repo remains private on the current plan.
- Public downloads still require either making the repo/release public or mirroring release assets to a public location.
Next:
- Push the guard fix to `core`, verify CI, and ask Channa whether to make the repo public or mirror the website/downloads separately.

## Run 2026-05-01T19:44:56Z
Phase: macOS first-run onboarding / DMG launch fix
Hats used: Engineering, CEO
Changed:
- [HAT: Engineering] Reproduced the first-run failure shape from an app bundle/DMG-like path: a fresh HOME had no user config/model, so `autowhisper run` failed with a missing model and Finder-style launch would appear to do nothing.
- [HAT: Engineering] Added macOS onboarding helpers so app-bundle launches create/copy a writable user config, request/prompt required macOS permissions, and open the native setup helper with the startup error instead of silently exiting.
- [HAT: Engineering] Expanded the native SwiftUI settings helper with a clear First-run setup panel: model download, Input Monitoring, Accessibility, and Microphone actions.
- [HAT: Engineering] Fixed the model-download setup action to drain subprocess output with `readabilityHandler` while waiting, avoiding pipe-buffer deadlock during large/slow downloads.
- [HAT: Engineering] Added static regression checks for app-bundle launch fallback, setup-helper launch, permission prompts, and first-run onboarding actions.
Verification:
- RED: Focused new macOS onboarding static tests failed before implementation.
- GREEN: `xcrun swiftc -target arm64-apple-macos12.0 -framework SwiftUI -framework AppKit -framework AVFoundation platform/macos/SettingsApp.swift` passed.
- GREEN: `cmake --build build-macos-onboarding` passed and rebuilt `AutoWhisper.app` plus bundled `AutoWhisperSettings`.
- GREEN: `python3 -m unittest discover tests/static -v` passed, 27/27 tests.
- GREEN: `ctest` in `build-macos-onboarding` passed, 117/117 tests.
- GREEN: `BUILD_DIR=$PWD/build-macos-onboarding ./scripts/macos_app_smoke.sh` passed; `spctl` rejection remains expected for this local non-notarized build.
- GREEN: Manual fresh-HOME run from `build-macos-onboarding/AutoWhisper.app/Contents/MacOS/autowhisper run` exited 0, created `~/.config/autowhisper/config.toml`, and launched `AutoWhisperSettings --setup-error` for the missing model.
- GREEN: Created a local DMG smoke artifact and mounted it; running the app executable from `/Volumes/AutoWhisper/AutoWhisper.app` exited 0, created user config, and launched the settings helper.
- GREEN: Independent second-pass review returned PASS with no blocking release risks after the subprocess pipe-drain fix.
Boundaries:
- This verifies local app-bundle/DMG-like launch behavior and ad-hoc/local signing smoke; it is not yet a notarized Developer ID release artifact.
- The setup helper opens the relevant macOS privacy panes and requests microphone/TCC prompts, but users may still need to manually toggle permissions in System Settings and relaunch after granting them.
Next:
- Commit on `primeodin/macos-first-run-onboarding`, push, open PR to `core`, and monitor CI.

## 2026-05-01T21:07:32Z — macOS downloaded release install reproduction and 0.7.1 release-prep evidence

- Reproduced the public `v0.7.0` macOS ZIP from GitHub release with a fresh download/unzip install-style flow.
- Evidence from clean HOME first run: Gatekeeper accepted the app, but first run exited `1`, did not create `~/.config/autowhisper/config.toml`, and stopped on missing `distil-small.en` before onboarding/setup. This matches user report that the downloaded app appears not to open and setup is unclear.
- Verified the existing first-run onboarding branch fix locally as `0.7.1`: clean HOME app-bundle launch exited `0`, created writable user config, and routed the missing-model failure to setup/onboarding instead of silent/non-zero failure.
- Captured browser screenshot evidence report at `/Users/odin-mac-730/.hermes/cache/screenshots/browser_screenshot_3ac93a4b7b304f3a9e72cc8d083adcef.png`; source report is `/tmp/autowhisper-install-screenshots/report.html`.
- Release-prep changes added on the onboarding branch: bumped CMake version to `0.7.1`, added `CHANGELOG.md` 0.7.1 notes, and updated static website download references/tests from `v0.7.0` to `v0.7.1`.
- Local gates after release-prep passed: CMake configure/build app bundle, static tests `27/27`, CTest `117/117`, `scripts/macos_app_smoke.sh`, `git diff --check`, app bundle plist version `0.7.1`, and CLI `--version` `0.7.1`.

## 2026-05-01T23:52:01Z — macOS v0.7.1 release published and public landing updated

- Confirmed user report: public macOS download was still `v0.7.0` until the release artifact and landing CTAs were updated.
- PR #10 was already merged to `core`; local `core` fast-forwarded to `093ee7a604f445e4c56e139789a3f9fef6bb321a`, with `project(autowhisper VERSION 0.7.1 ...)`.
- Built release app at `build-macos-release-0.7.1/AutoWhisper.app` with Developer ID signing and local Xcode 26.4.1 toolchain.
- Local gates passed before publishing: static tests `27/27`, CTest `117/117`, app smoke, codesign verification, bundle plist version `0.7.1`, and CLI `--version` `0.7.1`.
- Created `/tmp/AutoWhisper-macOS-v0.7.1.zip` with `ditto -c -k --keepParent`, submitted to Apple notarization, received `Accepted`, stapled the app, validated the staple, recreated the ZIP from the stapled app, and verified Gatekeeper accepted as `source=Notarized Developer ID`.
- Published GitHub release `v0.7.1` as latest with assets `AutoWhisper-macOS-v0.7.1.zip` and `.sha256`; direct URL `https://github.com/primemanifold/autowhisper/releases/download/v0.7.1/AutoWhisper-macOS-v0.7.1.zip`.
- Verified a fresh public release download: checksum OK, plist and CLI version `0.7.1`, Gatekeeper accepted, clean-HOME first run exited `0`, and user config was created.
- Release-event CI run `25238033133` completed success for build, build-source, and build-deb. Existing PPA workflow remains noisy/failing separately.
- Updated public landing repo `primemanifold/autowhisper-landing` commit `968274c` so homepage and roadmap CTAs point to `v0.7.1`; GitHub Pages run `25238071749` passed and hosted homepage/roadmap returned HTTP 200 with `v0.7.1` content.


## Run 2026-05-02T01:35:17Z
Phase: iOS app shell completion
Hats used: Engineering, CEO
Shipped:
- [HAT: Engineering] Created `primeodin/ios-swiftui-app-shell` from `core` for an isolated iOS app-shell slice.
- [HAT: Engineering] Added `ios/project.yml` as the XcodeGen source of truth and gitignored generated `ios/*.xcodeproj/` files.
- [HAT: Engineering] Added a runnable SwiftUI iOS app shell under `ios/AutoWhisperApp/` with launch screen, microphone usage description, privacy manifest, AutoWhisperCore dependency, foreground record/stop placeholder loop, copy/share transcript actions, and explicit iOS platform-limit copy.
- [HAT: Engineering] Updated `ios/README.md` and `engineering/ios-implementation-plan.md` to replace stale Xcode-blocked language with the new app-shell gate.
- [HAT: Engineering] Added static regression tests in `tests/static/test_ios_app_shell.py` and watched them fail before implementation, then pass.
Learned:
- [HAT: Engineering] Xcode 26.4.1, iPhoneOS26.4.sdk, iPhoneSimulator26.4.sdk, simctl, and XcodeGen are usable locally for iOS app-shell validation.
- [HAT: Engineering] Calling AVFoundation permission APIs on launch produced a system microphone prompt during screenshot QA; the shell now gates microphone permission behind an explicit `Request Microphone Permission` button.
Verification:
- RED: `python3 -m unittest tests.static.test_ios_app_shell -v` failed for missing `ios/project.yml`, iOS app metadata, SwiftUI shell, and updated README.
- GREEN: `swift run --package-path ios AutoWhisperCoreChecks` passed.
- GREEN: `python3 -m unittest discover -s tests/static -v` passed — 31/31 tests.
- GREEN: `xcodegen generate` passed from `ios/`.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived build` passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS' -sdk iphoneos CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived build` passed.
- GREEN: Simulator install + launch passed on iPhone simulator; screenshot captured at `/tmp/autowhisper-ios-evidence/ios-app-shell-final-erased.png` and verified the app shell is visible, unclipped, and does not auto-prompt for microphone permission.
- GREEN: `git diff --check` passed.
- Independent review returned PASS with no blockers; its only UX note about automatic microphone prompting was fixed and reverified after simulator erase.
Blocked on:
- No blocker for the iOS app-shell slice.
- Real iOS transcription remains next phase: bind AVFoundation recording buffers to `whisper.cpp`, add real model resources/loading, then validate simulator/device runtime and signing/TestFlight separately.
Next:
- Commit and push this branch, open PR into `core`.
- Next iOS slice should implement real AVFoundation recording and the first `whisper.cpp` inference bridge without claiming TestFlight/App Store readiness until signing/provisioning gates pass.


## Run 2026-05-02T01:59:11Z
Phase: iOS native-audio continuation on `primeodin/ios-swiftui-app-shell` / PR #12.
Changes:
- Added `IOSAudioRecorder` using AVFoundation/AVAudioRecorder for explicit-permission foreground recording.
- Recording target is 16 kHz mono 16-bit Linear PCM CAF in a temporary file, returning URL/duration metadata.
- Wired SwiftUI Start Recording / Stop & Transcribe through native recorder while retaining placeholder transcript copy and explicit "Whisper bridge next" messaging.
- Kept microphone permission behind explicit button; erased-simulator launch screenshot confirmed no automatic permission prompt.
- Updated iOS README, implementation plan, and static regression tests.
Verification:
- `swift run --package-path ios AutoWhisperCoreChecks`: passed.
- `python3 -m unittest discover -s tests/static -v`: 32/32 passed.
- `cd ios && xcodegen generate`: passed.
- `xcodebuild ... generic/platform=iOS Simulator ... CODE_SIGNING_ALLOWED=NO`: passed.
- `xcodebuild ... generic/platform=iOS ... CODE_SIGNING_ALLOWED=NO`: passed.
- `git diff --check`: passed.
- Simulator erase/install/launch screenshot: `/tmp/autowhisper-ios-evidence/ios-av-recorder-shell.png`.
- Independent review: PASS, no blockers; fixed non-blocking recorder-start failure cleanup by deactivating audio session if `record()` fails.
Limits:
- Still not real Whisper transcription, physical-device runtime, signed device install, TestFlight, or App Store readiness.

## Run 2026-05-02T03:07:35Z
Phase: iOS audio-decode bridge continuation on `primeodin/ios-swiftui-app-shell` / PR #12.
Changes:
- Added `IOSAudioDecoder` to decode recorded CAF files into normalized float PCM samples for the future inference bridge, with a preview guard for overly long recordings.
- Added async/sendable `IOSWhisperTranscribing` seam and `IOSPlaceholderWhisperTranscriber` so stop-recording output now passes through recorded-audio decode before returning honest bridge-pending UI output.
- Updated SwiftUI copy from "Stop & Transcribe"/"transcribe locally" to decode/bridge language that explicitly says real Whisper transcription is still the next iOS slice.
- Updated the iOS microphone permission prompt copy to decode/bridge language so the system permission sheet does not claim real local transcription yet.
- Fixed the `.preparingTranscript` state race by guarding duplicate toggle actions, changing the button title to `Decoding Audio…`, and disabling the recorder button while decode/transcriber work is pending.
- Added static regression checks for the decoder, transcriber seam, off-main decode path, no real-transcription overclaims while placeholder output remains active, and the `.preparingTranscript` button guard.
- Updated iOS README, implementation plan, and decisions/state docs.
Verification:
- RED: `python3 -m unittest tests.static.test_ios_app_shell -v` failed for missing `IOSAudioDecoder`, async transcriber seam, and overclaim guard expectations before implementation.
- GREEN: `swift run --package-path ios AutoWhisperCoreChecks`: passed.
- GREEN: `python3 -m unittest discover -s tests/static -v`: 33/33 passed.
- GREEN: `cd ios && xcodegen generate`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-bridge build`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS' -sdk iphoneos CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-bridge build`: passed.
- GREEN: `git diff --check`: passed.
- GREEN: Simulator erase/install/launch screenshot: `/tmp/autowhisper-ios-evidence/ios-audio-decode-bridge-shell.png`; vision verified the app is visible, no automatic microphone permission prompt appears, and hero copy avoids claiming real Whisper is implemented.
- Independent review initially requested changes for overclaiming copy, main-actor decode, microphone permission wording, and a `.preparingTranscript` duplicate-tap race; those issues were fixed and reverified.
- Final independent read-only review returned PASS after the race fix.
Limits:
- Still not real Whisper transcription, physical-device runtime, signed device install, TestFlight, or App Store readiness.

## Run 2026-05-02T10:39:11Z
Phase: iOS model-resource locator continuation on `primeodin/ios-swiftui-app-shell` / PR #12.
Changes:
- Added concrete GGML resource filenames to the iOS model catalog: `ggml-tiny.en.bin` and `ggml-base.en.bin`.
- Added `IOSWhisperModelLocator` to resolve bundled model URLs from `Bundle.main` under `Models/` or return a typed `missingBundledModel` error.
- Added `ModelResources.plist` plus `AutoWhisperApp/Models/README.md` and declared both resource locations in `ios/project.yml`; no large GGML binaries are committed in this slice.
- Updated `IOSPlaceholderWhisperTranscriber` to check the recommended model locator and include model-ready or `Model not bundled yet` state in bridge-pending placeholder output without calling `whisper.cpp`.
- Added static regression coverage for model filenames, resource declarations, model locator, no whisper API calls, and no real-inference overclaims.
- Updated iOS README, implementation plan, and decisions/state docs.
Verification:
- RED: `python3 -m unittest tests.static.test_ios_app_shell -v` failed before implementation because `ModelCatalog.swift` lacked `ggmlFilename` and the model locator/resources did not exist.
- GREEN: `swift run --package-path ios AutoWhisperCoreChecks`: passed.
- GREEN: `python3 -m unittest discover -s tests/static -v`: 34/34 passed.
- GREEN: `cd ios && xcodegen generate`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-modellocator build`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS' -sdk iphoneos CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-modellocator build`: passed.
- GREEN: `git diff --check`: passed.
- GREEN: Simulator erase/install/launch screenshot: `/tmp/autowhisper-ios-evidence/ios-model-locator-shell.png`; vision verified the app is visible, no automatic microphone permission prompt appears, and copy avoids claiming real Whisper is implemented.
- Independent read-only review returned PASS; no blockers or important issues.
Limits:
- Still no real bundled model binary, no `whisper.cpp` inference, no real transcript output, no physical-device runtime, no signed device install, no TestFlight, and no App Store readiness.

## Run 2026-05-02T16:00:38Z
Phase: iOS widget + small voice continuation on `primeodin/ios-swiftui-app-shell` / PR #12.
Changes:
- Performed a clean-room Ghost Pepper gap analysis for iOS voice entry points. Ghost Pepper remains architecture inspiration only because GitHub license metadata is null and no local `LICENSE*`/`COPYING*` file was found.
- Added `engineering/ios-widget-voice-plan.md` documenting the licensing boundary, WidgetKit platform constraints, implemented Quick Record slice, verification gates, and next slices.
- Added an `AutoWhisperWidget` WidgetKit app-extension target to `ios/project.yml` and embedded it in the iOS app target.
- Added a small `.systemSmall` Quick Record widget that opens `autowhisper://record` and does not touch microphone APIs in the widget process.
- Registered the `autowhisper` URL scheme in `AutoWhisperApp/Info.plist`.
- Wired `.onOpenURL` and `handleDeepLink(_:)` in `ContentView`/`AutoWhisperAppModel` so `autowhisper://record` opens the foreground app, requests/updates microphone permission if needed, and starts the existing AVFoundation foreground recorder only when authorized and idle.
- Added an AppIntent seam for future Shortcuts/interactivity while preserving iOS 16-compatible non-interactive widget behavior.
- Updated iOS README and static regressions to keep platform limits honest: widgets cannot record microphone audio directly, the widget opens the foreground app, and no Ghost Pepper code is vendored or copied.
Verification:
- RED: targeted static tests failed before implementation because the widget target, deep link, widget files, and docs did not exist.
- GREEN: `python3 -m unittest tests.static.test_ios_app_shell -v`: 9/9 passed.
- GREEN: `swift run --package-path ios AutoWhisperCoreChecks`: passed.
- GREEN: `cd ios && xcodegen generate`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-widget build`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS' -sdk iphoneos CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-widget-device build`: passed.
- GREEN: `git diff --check`: passed.
- GREEN: Simulator install/launch/open-url smoke captured `/tmp/autowhisper-ios-evidence/ios-widget-deeplink-record.png`; vision verified the app is visible, copy is honest about real Whisper still being next, and no microphone permission prompt is visible. Non-blocking: `simctl openurl` shows the expected system “Open in AutoWhisper?” confirmation modal, so this is not yet full widget-tap runtime proof.
- Independent read-only review returned PASS; no licensing/claim-boundary/platform-risk blockers.
Limits:
- Still no real Whisper transcription, no widget/background microphone capture, no Ghost Pepper code reuse, no physical-device runtime proof, no signed device install, no TestFlight, and no App Store readiness.
Next:
- Commit/push this slice if final precommit stays clean; next runtime proof should be a real widget-tap or accepted deep-link flow on simulator/physical device.

## Run 2026-05-02T16:31:20Z
Phase: plan-only cross-platform readiness PRD + Claude batch prompt pack on `primeodin/ios-swiftui-app-shell`.
Changes:
- Added `engineering/cross-platform-readiness-prd.md` as the discussion spec for a macOS/Linux/Windows/mobile readiness foundation.
- Added `engineering/claude-cross-platform-batch-plan.md` with read-only scout prompts, model selection, implementation lanes, verification gates, and stop conditions.
- Incorporated README usage-guide/media planning into the PRD/batch plan, including screenshot captions, `docs/usage-guide.md`, `docs/media/`, and a future Manim explainer video plan.
- This was intentionally plan-only: no platform implementation, release link changes, public release creation, or Claude batch launch.
Verification:
- GREEN: custom Python doc smoke verified required sections/snippets in both new docs.
- GREEN: `python3 -m unittest discover -s tests/static -v`: 36/36 passed.
- GREEN: `git diff --check`: passed.
Limits:
- New docs are uncommitted draft files for discussion.
- No read-only Claude scout batch has been launched yet.
- No new platform readiness status is proven by this planning slice.
Next:
- Review with Channa, then if approved launch Phase 1 read-only Claude scouts before implementation.

## Run 2026-05-02T18:44:43Z
Phase: cross-platform readiness Lane A + Lane B docs/tests on `primeodin/ios-swiftui-app-shell` / PR #12.
Changes:
- Ran Phase 1 read-only Claude scouts for macOS, Linux, Windows, iOS/mobile, and product docs; saved reports under `engineering/claude-scout-reports/` with a synthesis recommending Lane A + Lane B first.
- Committed the plan/scout pack as `f98839f` (`docs: plan cross-platform readiness batch`).
- Added the readiness foundation: `engineering/platform-readiness-matrix.md`, `engineering/platform-validation-commands.md`, `docs/usage-guide.md`, `docs/media/README.md`, and `docs/media/manim/autowhisper-flow-plan.md`.
- Updated README with concise local-first and per-platform status links/copy.
- Added static claim-boundary tests in `tests/static/test_platform_readiness_docs.py` and `tests/static/test_docs_claims.py`.
- Committed the implementation as `ac07830` (`docs: add platform readiness matrix and guide`).
Verification:
- GREEN: control-character scan passed after fixing hidden BEL bytes in Windows PowerShell examples.
- GREEN: `python3 -m unittest tests.static.test_platform_readiness_docs tests.static.test_docs_claims -v`: 16/16 passed.
- GREEN: `python3 -m unittest discover -s tests/static -v`: 52/52 passed.
- GREEN: `git diff --check`: passed.
- Independent review initially requested changes for the hidden control-character blocker; follow-up independent review returned PASS after the fix.
Limits:
- This slice is docs/tests-only and proves claim hygiene, not new runtime platform capability.
- macOS public artifact language remains limited to v0.7.1; v0.7.2 remains candidate/PR until public artifact validation.
- Windows remains build-proven/runtime-unverified; iOS still has no real local Whisper transcription or TestFlight/App Store readiness.
Next:
- Push the local commits to `origin/primeodin/ios-swiftui-app-shell` to update PR #12, then watch hosted checks.

## Run 2026-05-02T20:56:02Z
Phase: iOS first-run microphone permission hardening on `primeodin/ios-swiftui-app-shell` / PR #12.
Changes:
- Used read-only scouts to choose the next small reversible iOS slice after the green PR #12 docs/widget foundation.
- Updated `AutoWhisperAppModel.requestMicrophonePermission()` to return whether permission was granted while preserving existing button/deep-link callers.
- Added `hasMicrophonePermissionForRecording()` so the primary Start Recording path requests/verifies microphone permission before `IOSAudioRecorder.startRecording()`.
- Denied/restricted/unavailable microphone states now remain non-recording and surface actionable foreground-app copy instead of falling through to recorder failure.
- Updated iOS README and static regression tests for the primary record-button permission gate.
Verification:
- RED: `python3 -m unittest tests.static.test_ios_app_shell.IOSAppShellTests.test_ios_primary_record_button_requests_microphone_permission_first -v` failed before implementation because the permission gate did not exist.
- GREEN: `python3 -m unittest tests.static.test_ios_app_shell -v`: 10/10 passed.
- GREEN: `swift run --package-path ios AutoWhisperCoreChecks`: passed.
- GREEN: `cd ios && xcodegen generate`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-permission build`: passed.
- GREEN: `xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS' -sdk iphoneos CODE_SIGNING_ALLOWED=NO -derivedDataPath /tmp/autowhisper-ios-derived-permission-device build`: passed.
- GREEN: `python3 -m unittest discover -s tests/static -v`: 53/53 passed.
- GREEN: `git diff --check`: passed.
- Independent read-only review returned PASS; no blockers or important issues.
Limits:
- This is first-run/onboarding hardening only; it does not prove physical-device microphone runtime, widget tap behavior, real Whisper transcription, model bundling, TestFlight, or App Store readiness.
- Widget/deep-link behavior remains foreground-app only; no widget/background microphone capture is claimed.
Next:
- Commit/push this slice if final preflight stays clean, then watch hosted checks.

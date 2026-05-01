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

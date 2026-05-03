# Claude Cross-Platform Batch Plan

> **For Hermes:** This file is the prompt pack for a future Claude Code batch. Do not execute implementation automatically until Channa approves the selected lane. Start with read-only scouts, synthesize, then ask for go/no-go.

**Repo:** `/Users/odin-mac-730/src/autowhisper-macos-app-gate`
**Base planning doc:** `engineering/cross-platform-readiness-prd.md`
**Quality bar:** Evidence-backed, release-grade, no platform overclaims.

## 1. Batch objective

Create a cross-platform readiness foundation for AutoWhisper that covers macOS, Linux, Windows, and mobile/iOS, plus a clear user-facing README/guide/media plan.

The first implementation after approval should be small and high-leverage: platform matrix, validation command pack, claim-boundary tests, and usage-guide scaffolding. Do not attempt full feature parity across all platforms in one batch.

## 2. Claude model policy

| Lane | Model | Effort | Why |
| --- | --- | --- | --- |
| macOS release/readiness scout | Sonnet | high | Signing/notarization and first-run proof need careful evidence boundaries. |
| Linux install/runtime scout | Sonnet | medium/high | Linux is core product but needs reproducible commands. |
| Windows feasibility scout | Sonnet, escalate Opus if needed | high | Buildability/runtime separation and CMake/Win32 seams are easy to overclaim. |
| iOS/mobile scout | Sonnet | high | Swift/iOS platform constraints and current PR #12 state need precision. |
| Product claims + README guide scout | Haiku or Sonnet | medium | Mostly docs/copy/static analysis; escalate if claim conflicts appear. |
| Synthesis | Sonnet high or Opus | high/max | Merge platform reports into one coherent plan. |
| Final review | Opus or Sonnet max | max | Catch release/legal/platform-boundary mistakes before implementation. |

## 3. Execution modes

### Preferred for read-only scouts

Use Claude print mode with restricted tools:

```bash
claude -p "<SCOUT PROMPT>" \
  --model sonnet \
  --effort high \
  --allowedTools "Read,Bash(git status *),Bash(git diff *),Bash(git ls-files *),Bash(python3 *),Bash(cmake --version),Bash(xcodebuild -version),Bash(swift --version),Bash(gh pr view *),Bash(gh release view *)" \
  --max-turns 8 \
  --output-format json
```

Scouts must not edit files.

### Preferred for implementation lanes

Use isolated Claude worktrees or sequential print-mode tasks. Avoid parallel edits to the same files.

```bash
claude -p "<IMPLEMENTATION PROMPT>" \
  --model sonnet \
  --effort high \
  --allowedTools "Read,Edit,Write,Bash" \
  --max-turns 15 \
  --fallback-model haiku
```

If using interactive `/batch`, require:

- one worktree per lane,
- no lane touches `.hermes/state.md` except the final integration lane,
- no lane pushes to GitHub,
- all lanes produce a summary and test evidence.

## 4. Shared constraints for every prompt

Include this block in every Claude prompt:

```text
You are working on AutoWhisper in /Users/odin-mac-730/src/autowhisper-macos-app-gate.
Read engineering/cross-platform-readiness-prd.md first.
Do not expose secrets or credentials.
Do not copy Ghost Pepper code; license is unclear.
Do not claim Windows runtime support from MinGW cross-build alone.
Do not claim iOS real local transcription until a real whisper.cpp bridge produces transcript output.
Do not claim iOS widgets record audio directly; they only open the foreground app.
Do not claim macOS public release fixes until a downloaded release artifact is validated.
Prefer small, reversible changes with tests.
Report proof levels separately: local build, package, public artifact, install/first-run, runtime smoke, E2E.
```

## 5. Phase 1 — read-only scout prompts

### Scout A — macOS release/readiness

**Model:** Sonnet high

```text
Read-only task. Inspect AutoWhisper macOS readiness.

Files/areas to inspect:
- README.md
- engineering/macos-roadmap.md
- engineering/desktop-platform-foundation.md
- tests/static/test_macos_app_bundle_assets.py
- cmake/MacOSBundle.cmake
- platform/macos/
- scripts/macos_app_smoke.sh
- CHANGELOG.md
- GitHub release references if available through gh.

Output:
1. Current macOS proof levels: source build, app bundle, signing, notarization, public artifact, first-run, model download, runtime E2E.
2. Top 5 gaps preventing a stronger macOS release claim.
3. Smallest safe implementation slice for macOS in this batch.
4. Exact commands to verify the slice.
5. Claim-boundary tests that should exist.

Do not edit files.
```

### Scout B — Linux install/runtime

**Model:** Sonnet medium/high

```text
Read-only task. Inspect AutoWhisper Linux readiness.

Files/areas to inspect:
- README.md
- debian/
- autowhisper.service
- build-deb.sh
- scripts/
- src/doctor/
- src/settings/
- src/hotkey/
- src/output/
- tests/

Output:
1. Current Linux proof levels: source build, package, PPA/install, service start, doctor, model management, hotkey, text insertion, runtime E2E.
2. Top 5 Linux install/runtime documentation or verification gaps.
3. Smallest safe Linux readiness slice.
4. Commands that prove Linux build/static readiness and commands that require real X11 desktop runtime.
5. README/usage guide screenshots or captions needed for Linux users.

Do not edit files.
```

### Scout C — Windows feasibility

**Model:** Sonnet high, escalate to Opus if platform abstractions are tangled

```text
Read-only task. Inspect AutoWhisper Windows status.

Files/areas to inspect:
- engineering/desktop-platform-foundation.md
- cmake/toolchains/
- CMakeLists.txt
- platform/windows/ or src/*/platform/*windows*
- scripts/desktop_docker_smoke.sh
- tests that mention Windows/platform capabilities.

Output:
1. Current Windows proof levels: source/cross-build, PE artifact, Wine runtime, actual Windows runtime, installer/startup.
2. Which features are ready/partial/unsupported/unverified: microphone, hotkey, tray, clipboard/text insertion, model management, settings UI, service/startup.
3. Smallest safe Windows readiness slice that avoids overclaiming.
4. Commands for MinGW cross-build and optional Wine/runtime proof.
5. Static tests that should prevent Windows overclaims.

Do not edit files.
```

### Scout D — iOS/mobile readiness

**Model:** Sonnet high

```text
Read-only task. Inspect AutoWhisper iOS/mobile readiness on PR #12.

Files/areas to inspect:
- ios/README.md
- ios/project.yml
- ios/AutoWhisperApp/
- ios/AutoWhisperWidget/
- ios/Sources/AutoWhisperCore/
- engineering/ios-implementation-plan.md
- engineering/ios-widget-voice-plan.md
- tests/static/test_ios_app_shell.py

Output:
1. Current mobile proof levels: Swift package, app shell, simulator build, generic device build, physical-device proof, widget/deep-link proof, real inference, TestFlight/App Store.
2. Top 5 next mobile gaps.
3. Smallest safe iOS slice after current widget launcher.
4. Commands for simulator/generic device proof, and what they do not prove.
5. Claim-boundary tests to keep widget and local transcription language honest.

Do not edit files.
```

### Scout E — Product claims, README guide, and media plan

**Model:** Haiku or Sonnet medium

```text
Read-only task. Inspect AutoWhisper user-facing docs and plan a clear guide with images/captions/videos.

Files/areas to inspect:
- README.md
- docs/
- site/
- engineering/design-system-audit.md
- engineering/cross-platform-readiness-prd.md
- tests/static/test_website_assets.py

Output:
1. User-facing claims that may overstate platform support or readiness.
2. README quick-start guide outline with screenshot captions.
3. docs/usage-guide.md outline.
4. docs/media inventory proposal for screenshots, GIFs, short videos.
5. Manim explainer video concept: narrative arc, scenes, captions, target duration, output path.
6. Static checks that should enforce guide/claim honesty.

Do not edit files.
```

## 6. Phase 2 — synthesis prompt

**Model:** Sonnet high or Opus

```text
You are synthesizing five read-only scout reports for AutoWhisper's cross-platform readiness foundation.

Inputs:
- engineering/cross-platform-readiness-prd.md
- Scout reports for macOS, Linux, Windows, iOS/mobile, and product/docs/media.

Produce:
1. Final recommended implementation lane order.
2. A risk-adjusted scope for the first implementation batch only.
3. Exact files to create/modify.
4. Tests to add/run.
5. Commands to verify locally before commit.
6. Stop conditions requiring Channa approval.

Do not edit files in synthesis unless explicitly instructed. Prefer a concise execution memo.
```

## 7. Phase 3 — candidate implementation lanes

Implementation starts only after Channa approves one of these.

### Lane A — Matrix, validation commands, claim-boundary tests

**Model:** Sonnet high

Files:

- Create/modify: `engineering/platform-readiness-matrix.md`
- Create/modify: `engineering/platform-validation-commands.md`
- Create/modify: `tests/static/test_platform_readiness_docs.py`
- Modify if needed: `engineering/cross-platform-readiness-prd.md`

Acceptance:

- Matrix covers macOS, Linux, Windows, iOS.
- Every status uses the readiness vocabulary from the PRD.
- Every command states what it proves and does not prove.
- Static tests reject Windows runtime overclaim, iOS transcription overclaim, widget recording overclaim, and macOS public artifact overclaim.

### Lane B — README usage guide + media scaffolding

**Model:** Sonnet high; Haiku acceptable for caption-copy only

Files:

- Modify: `README.md`
- Create: `docs/usage-guide.md`
- Create: `docs/media/README.md`
- Create: `docs/media/manim/autowhisper-flow-plan.md`
- Modify/create: static tests for README media/caption honesty.

Acceptance:

- README shows a beginner-friendly quick start with captions.
- Linux, macOS, Windows, and iOS sections are honest about status.
- Media placeholders use explicit TODO/evidence language, not fake screenshots.
- Manim plan includes narrative arc, scene list, captions/subtitles, target output path, and verification commands.

### Lane C — macOS/Linux validation script hardening

**Model:** Sonnet high

Files:

- Existing scripts under `scripts/`
- Existing macOS static tests
- New docs in `engineering/platform-validation-commands.md`

Acceptance:

- macOS and Linux commands are copy-pasteable.
- Scripts do not require secrets.
- macOS proof boundaries distinguish local ad-hoc/development proof from Developer ID/notarized public proof.
- Linux proof boundaries distinguish Docker/build proof from real X11 runtime proof.

### Lane D — Windows readiness boundary

**Model:** Sonnet high; Opus if CMake/platform abstractions fail

Files:

- `engineering/desktop-platform-foundation.md`
- `scripts/desktop_docker_smoke.sh`
- CMake/toolchain files if needed
- platform capability tests

Acceptance:

- Windows status cannot be accidentally reported as runtime-ready from cross-build only.
- Cross-build commands are documented and tested if environment supports them.
- Wine/Windows runtime remains explicitly pending unless actually executed.

### Lane E — iOS physical-proof preparation

**Model:** Sonnet high

Files:

- `engineering/ios-implementation-plan.md`
- `engineering/ios-widget-voice-plan.md`
- `ios/README.md`
- iOS static tests if needed

Acceptance:

- Physical-device proof checklist exists.
- Widget/deep-link proof path is clear.
- Real `whisper.cpp` bridge remains separate.
- No TestFlight/App Store claims without proof.

## 8. Recommended first batch after discussion

Run these in order, not all at once:

1. Phase 1 scouts A-E in parallel.
2. Synthesis prompt.
3. If Channa approves, implement Lane A and Lane B first.
4. Run local static/docs verification.
5. Independent review.
6. Commit locally; push only after deliberate approval.

## 9. Final verification gates for first implementation batch

At minimum:

```bash
python3 -m unittest discover -s tests/static -v
git diff --check
git status --short --branch
```

If touched areas require it:

```bash
swift run --package-path ios AutoWhisperCoreChecks
cd ios && xcodegen generate
xcodebuild -project AutoWhisperIOS.xcodeproj -scheme AutoWhisperApp -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO build
cmake -S . -B build-audit -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build-audit --target autowhisper autowhisper_tests
ctest --test-dir build-audit --output-on-failure
```

## 10. Stop conditions

Stop and ask Channa before:

- launching implementation after read-only scout/synthesis;
- changing release/download links;
- pushing to GitHub;
- creating a public release;
- adding dependencies;
- copying or vendoring external code;
- making Windows/iOS support claims stronger than current proof.

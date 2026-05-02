# AutoWhisper Cross-Platform Readiness Foundation PRD

> **For Hermes:** This is a plan/spec only. Do not launch implementation until Channa approves the chosen lane. When implementation starts, use the `claude-code`, `subagent-driven-development`, `test-driven-development`, and `cross-platform-desktop-build-validation` skills.

**Status:** Draft for discussion  
**Owner:** Channa / Hermes Operator  
**Primary repo:** `primemanifold/autowhisper`  
**Local path:** `/Users/odin-mac-730/src/autowhisper-macos-app-gate`  
**Current planning branch:** `primeodin/ios-swiftui-app-shell`

## 1. Product goal

Make AutoWhisper's macOS, Linux, Windows, and mobile platform status explicit, testable, and evidence-backed, while adding only the smallest readiness improvements needed to prevent overclaiming and guide the next release.

The outcome should make AutoWhisper feel like a serious cross-platform product program without pretending that every platform has equal maturity.

## 2. Current platform truth

| Platform | Current evidence | Honest status | Main gap |
| --- | --- | --- | --- |
| macOS | Public notarized `v0.7.1`; local Developer ID/notary tooling exists; app icon/settings download UX fixes in PR #11 | Strongest non-Linux public path, but release UX still needs careful artifact-level proof per version | First-run/setup/release proof must stay tied to actual shipped artifact |
| Linux | README/PPA install path; Linux-first C++ daemon design; X11/hotkey/text insertion model | Original product core | Needs reproducible install/runtime proof and stronger operator-facing readiness diagnostics |
| Windows | Prior desktop foundation doc says Docker/MinGW cross-build can prove PE artifacts only | Weak/unverified runtime | Needs explicit buildability/runtime boundary, not marketing support claims |
| iOS/mobile | PR #12 has SwiftUI shell, AVFoundation foreground recorder, PCM decode seam, model locator seam, Quick Record widget launcher | Early mobile sibling surface | Needs physical-device proof and real `whisper.cpp` inference before local transcription claims |

## 3. North-star experience

A user, operator, or reviewer should be able to answer this in under one minute:

1. Which platforms are genuinely ready today?
2. Which platforms build but are not runtime-proven?
3. Which features are unavailable because of OS constraints?
4. Which commands prove each claim?
5. What is the next honest milestone for each platform?

## 4. Product principles

- **Evidence over aspiration.** Every platform claim maps to a command, artifact, screenshot, release, or runtime proof.
- **Fail closed on readiness.** Unknown/unverified platform states are not warnings hidden behind success; they are explicit `unverified`, `unsupported`, or failing readiness gates.
- **Local-first remains core.** Desktop and mobile work should not add cloud transcription dependencies.
- **Platform-native, not behavior cloning.** iOS is not a desktop daemon; Windows should not inherit Linux assumptions; macOS should feel native where user-facing.
- **Small reversible slices.** No broad rewrites, no dependency sprawl, no Ghost Pepper code copying.
- **Separate proof levels.** Buildability, packaging, install, runtime smoke, and true E2E transcription are different statuses.

## 5. Non-goals for the next batch

- Do not implement full Windows runtime support in one sweep.
- Do not claim Windows E2E from MinGW cross-build alone.
- Do not implement full iOS `whisper.cpp` transcription unless explicitly selected as a separate follow-up.
- Do not ship a new public macOS release as part of this planning batch.
- Do not redesign all desktop/mobile UI surfaces.
- Do not copy Ghost Pepper code. Do not vendor or fork Ghost Pepper code into AutoWhisper until licensing/permission is clear.
- Do not trigger repeated remote CI from exploratory commits; prefer local verification, then deliberate push.

## 6. Readiness vocabulary

Use these exact states across docs, tests, settings UI, and diagnostics:

| State | Meaning |
| --- | --- |
| `ready` | Implemented and runtime-proven on the named platform. |
| `partial` | Some implementation exists, but at least one runtime/install/E2E gate is missing. |
| `build_proven` | Compiles or emits artifacts, but runtime was not executed on target OS. |
| `packaged` | Produces a package/app bundle, but install/first-run may still be unproven. |
| `release_proven` | Public downloadable artifact was fetched and validated after publication. |
| `unsupported` | The platform or OS policy does not support the behavior. |
| `unverified` | Unknown or not yet tested; must not be marketed as supported. |
| `planned` | Product direction only; no readiness claim. |

## 7. Platform acceptance criteria

### 7.1 macOS

Minimum next-batch acceptance:

- A macOS row in the platform matrix separates:
  - local build proof,
  - `.app` bundle proof,
  - Developer ID signing/notarization proof,
  - public artifact proof,
  - first-run/model-download proof.
- Any docs that mention a fixed public macOS release must name the exact tag/asset and validation command.
- Local verification commands are documented for:
  - CMake build/tests,
  - `.app` smoke script,
  - codesign verification,
  - Gatekeeper assessment when using Developer ID artifacts.
- No doc claims a new public macOS fix until a release asset has been downloaded and validated after publication.

Deferred:

- New notarized release cut.
- Full native UI redesign.
- New model download architecture beyond existing PR #11 unless explicitly chosen.

### 7.2 Linux

Minimum next-batch acceptance:

- Linux readiness defines separate proof for:
  - source build,
  - package/PPA install path,
  - service start,
  - `autowhisper doctor`,
  - model availability/download,
  - hotkey and text insertion runtime.
- A reproducible Linux validation command exists, preferably Docker for build/static proof plus explicit note for real desktop/X11 runtime proof.
- Linux remains the reference production desktop flow unless a later ADR changes product strategy.

Deferred:

- Full PPA release rebuild.
- Wayland parity.
- CUDA performance benchmarking.

### 7.3 Windows

Minimum next-batch acceptance:

- Windows claims separate:
  - MinGW cross-build proof,
  - Wine runtime proof if executed,
  - actual Windows VM/device proof if executed.
- If only cross-build exists, status is `build_proven`, not `ready`.
- Settings/docs/doctor output must not imply Windows hotkey, tray, text injection, startup integration, or installer readiness unless those paths exist and are runtime-proven.
- A future Windows task list identifies concrete seams: Win32 hotkey, clipboard/SendInput, tray, settings surface, installer/startup, model path, microphone runtime.

Deferred:

- Native Windows installer.
- Actual Windows E2E unless a Windows VM/runner/device is available.

### 7.4 iOS/mobile

Minimum next-batch acceptance:

- Mobile readiness explicitly says:
  - runnable SwiftUI shell: yes,
  - foreground AVFoundation recorder: yes,
  - PCM decode seam: yes,
  - model locator seam: yes,
  - Quick Record widget launcher: yes,
  - real `whisper.cpp` transcript: no,
  - widget/background recording: unsupported,
  - physical-device runtime: pending,
  - TestFlight/App Store: pending.
- The next iOS proof plan separates simulator build, generic device build, signed physical-device install, microphone permission runtime, widget-tap/deep-link runtime, and real inference.

Deferred:

- Real `whisper.cpp` iOS bridge.
- TestFlight.
- App Store privacy review.

## 8. Required deliverables for the next batch

### D1 — Platform readiness matrix

Create or update:

- `engineering/platform-readiness-matrix.md`

The matrix must include one row per platform and one column per proof level:

- source build,
- package/app bundle,
- public artifact,
- install/first-run,
- microphone capture,
- hotkey/quick launch,
- text output/copy/share,
- model management,
- local transcription,
- runtime E2E,
- next milestone.

### D2 — Machine-readable readiness contract

Either document the existing contract or add a small canonical JSON shape for future implementation.

Suggested shape:

```json
{
  "platform": "macos",
  "source_build": "ready",
  "package": "packaged",
  "public_artifact": "release_proven",
  "microphone": "partial",
  "quick_launch": "partial",
  "text_output": "partial",
  "model_management": "partial",
  "local_transcription": "partial",
  "runtime_e2e": "partial",
  "notes": ["Evidence must cite exact command or artifact."]
}
```

### D3 — Validation command pack

Create or update:

- `engineering/platform-validation-commands.md`

Must list commands for:

- macOS host-local build/package proof,
- Linux build/package/doctor proof,
- Windows cross-build and optional runtime proof,
- iOS simulator/generic-device build proof,
- static claim-boundary tests.

Each command must state what it proves and what it does **not** prove.

### D4 — Claude batch prompt pack

Create:

- `engineering/claude-cross-platform-batch-plan.md`

Must include:

- scout prompts,
- implementation-lane prompts,
- model selection policy,
- worktree strategy,
- verification gates,
- stop conditions.

### D5 — Static claim-boundary tests

Add or plan tests that prevent regressions such as:

- iOS docs claiming real local transcription before `whisper.cpp` bridge exists.
- widget docs claiming direct/background microphone recording.
- Windows docs claiming runtime readiness from cross-build-only proof.
- macOS public download docs claiming a version is fixed before release proof exists.

### D6 — README usage guide and media plan

Create or plan a user-facing guide that makes AutoWhisper easy to understand quickly:

- README quick-start section with captions and screenshots for install, model setup, recording, settings, doctor, logs, and platform-specific limits.
- `docs/usage-guide.md` for deeper walkthroughs.
- `docs/media/` inventory for screenshots, short GIF/video clips, and future Manim explainers.
- A Manim narrative plan for one short explainer video: "How AutoWhisper turns a hotkey recording into local text".
- Static checks that README captions do not overclaim platform support.

This guide is part of platform readiness because users need to know exactly what works on macOS, Linux, Windows, and mobile.

## 9. Claude batch execution strategy

### Phase 1 — Read-only scouts

Run these before implementation. They may read files and run non-mutating inspection commands. They must not edit files.

1. **macOS Release/Readiness Scout** — Sonnet high effort.
2. **Linux Install/Runtime Scout** — Sonnet medium/high effort.
3. **Windows Feasibility Scout** — Sonnet high; Opus only if platform abstractions are tangled.
4. **iOS/Mobile Scout** — Sonnet high effort.
5. **Product Claims Scout** — Haiku or Sonnet medium effort.

### Phase 2 — Synthesis

Use Sonnet high or Opus to synthesize scout reports into final PRD adjustments and task ordering.

### Phase 3 — Implementation lanes

Only after Channa approves the synthesized plan:

- Lane A: docs/matrix/claim-boundary tests.
- Lane B: macOS/Linux validation scripts and docs.
- Lane C: Windows buildability/readiness boundaries.
- Lane D: iOS quick-record/physical-device proof preparation.

Avoid parallel writes to the same files. If two lanes need the same file, serialize them.

## 10. Model selection policy

| Work type | Default model | Escalate to Opus when |
| --- | --- | --- |
| Formatting docs, tables, grep-style claim scans | Haiku | Cross-platform conclusions conflict |
| Normal implementation/tests/scripts | Sonnet | Platform architecture is ambiguous |
| CMake/Windows portability | Sonnet high | Multiple failing build systems or API seams conflict |
| iOS SwiftUI/AVFoundation polish | Sonnet high | Real inference/concurrency/model lifecycle is chosen |
| Final synthesis/review | Sonnet high | Release strategy or legal/licensing risk is central |

## 11. Risks

| Risk | Mitigation |
| --- | --- |
| Claude batch overbuilds across all platforms | Use read-only scouts first; require explicit approval before implementation. |
| Windows gets marketed as ready too early | Use `build_proven` vs `ready`; static tests enforce language. |
| iOS widget is overclaimed | Keep widget described as launcher only; recording stays foreground app. |
| macOS release claims drift from actual assets | Require exact tag/asset/download validation before public claims. |
| Ghost Pepper licensing risk | Architecture inspiration only; no copied code; no fork-as-base until license is clear. |
| CI spam | Prefer local gates; push only deliberate branch checkpoints. |

## 12. Recommended first implementation slice after PRD approval

Slice name: Platform readiness matrix + validation command pack + claim-boundary tests.

Why this first:

- High leverage.
- Low implementation risk.
- Gives all platforms coverage.
- Makes future Claude lanes safer.
- Prevents marketing/product drift.

Acceptance:

- `engineering/platform-readiness-matrix.md` exists.
- `engineering/platform-validation-commands.md` exists.
- Static tests cover the worst claim-boundary regressions.
- Existing iOS/macOS/Linux tests still pass locally.
- No platform is promoted to `ready` without corresponding evidence.

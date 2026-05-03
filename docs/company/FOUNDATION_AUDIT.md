# Foundation Audit

Last updated: 2026-05-03

## What exists

- **Core desktop app:** C++20/CMake app with CLI, daemon, config validation, settings server, audio capture, whisper.cpp inference, hotkey handling, output injection, tray/service helpers, and doctor checks.
- **Linux runtime path:** strongest implemented path, especially Ubuntu/X11 push-to-talk dictation.
- **macOS path:** app bundle, native onboarding/settings/model work, platform implementations, signing/notarization capability, and public ZIP release process in progress.
- **iOS foundation:** SwiftUI app shell, foreground AVFoundation recorder, PCM decode seam, placeholder transcriber seam, model resource locator, and widget/deep-link launcher.
- **CI and tests:** Python static tests, C++ CMake/CTest path, Swift package checks, and hosted macOS iOS app/widget build job.
- **Product evidence discipline:** platform readiness matrix, validation command pack, claim-boundary tests, and Hermes state/decision logs already exist.

## What is missing

- Reproducible latency/accuracy benchmark harness.
- Real iOS whisper.cpp transcription bridge and physical-device runtime proof.
- iOS interruption/background recorder reconciliation.
- Mature cross-platform first-run activation evidence.
- Product analytics or user feedback loop.
- Deep source-linked competitor research beyond the seeded baseline.
- Unified cross-platform recording-state component spec.

## What is fragile

- iOS readiness can look stronger than it is because many checks are static or build-only.
- Platform breadth creates claim risk: Windows build proof is not runtime support, and iOS app shell is not production transcription.
- Marketing speed/quality claims need benchmark backing.
- Local-first privacy posture will become fragile if cloud cleanup/context features are added without explicit disclosure.

## What is unusually strong

- The repo already separates evidence levels instead of making blanket platform claims.
- The app has a credible local-first C++/whisper.cpp core.
- CI now catches iOS app/widget build regressions on hosted macOS runners.
- Recent macOS release hardening creates a path for non-CLI users.
- Hermes campaign state and decision logs provide continuity across sessions.

## Likely competitor gaps/opportunities

Seeded public research suggests competitors emphasize polished dictation, AI cleanup, cross-app use, multilingual support, privacy/local modes, meeting summaries, transcript libraries, and contextual rewriting. AutoWhisper's strongest possible gaps/opportunities are:

- **Trust through evidence:** publish platform and benchmark proof instead of vague claims.
- **Local/private workflows:** make local processing, retention, and export explicit and user-owned.
- **Developer/operator workflows:** transform speech into structured docs, issues, commands, and agent prompts.
- **Activation quality:** beat open-source friction with native onboarding and first-run reliability.
- **Cross-platform honesty:** ship fewer claims, but make each one provable.

## Recommended first five PR-sized improvements

| Rank | Improvement | Impact | Confidence | Effort | Strategic leverage |
|---:|---|---:|---:|---:|---:|
| 1 | Implement iOS interruption/background recorder reconciliation | 4 | 4 | 3 | 4 |
| 2 | Add a lightweight transcription benchmark harness and first baseline docs | 5 | 3 | 3 | 5 |
| 3 | Add macOS downloaded-ZIP first-run smoke script and artifact evidence doc | 5 | 4 | 3 | 5 |
| 4 | Convert iOS static state checks into Swift/XCTest-style injectable tests | 4 | 3 | 3 | 4 |
| 5 | Run a deeper competitor/pricing/review scan and update gap ledger priorities | 3 | 4 | 2 | 4 |

## What should not be touched yet

- Do not build a large meeting-transcription/library product until the wedge is chosen.
- Do not make broad UI rewrites outside the design-system workflow.
- Do not market iOS, Windows, or mobile as production-ready.
- Do not add cloud cleanup/context features before privacy documentation and opt-in UX exist.
- Do not chase every competitor feature before activation and benchmark proof are stronger.

## Next best action

The highest-leverage reversible engineering task is iOS interruption/background recorder reconciliation because it directly reduces trust risk in the current PR #12 iOS shell. The highest-leverage product task is the benchmark harness because it turns speed/quality claims into evidence.

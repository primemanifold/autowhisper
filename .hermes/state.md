# AutoWhisper Hermes State

Current phase: Phase 0 — Orient
Repository: `primemanifold/autowhisper`
Local path: `/Users/odin-mac-730/src/autowhisper`
Branch: `core`
Observed commit: `2778945`
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

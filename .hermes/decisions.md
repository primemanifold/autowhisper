# AutoWhisper Decisions

Append-only ADR log. Do not silently delete or rewrite prior decisions.

## ADR-0001 — Initialize Hermes campaign state and Phase 0 substrate

Date: 2026-04-30T14:28:38Z

### Context

The Hermes Operator campaign prompt requires `.hermes/state.md` and `.hermes/decisions.md` to exist before action, and requires Phase 0 orientation artifacts before feature, roadmap, or competitor-parity work. The repository had no `.hermes/` campaign files and no `engineering/architecture.md` or `engineering/design-system-audit.md`.

### Decision

Initialize `.hermes/state.md`, `.hermes/decisions.md`, `.hermes/roadmap.md`, `.hermes/reviews/`, `engineering/architecture.md`, and `engineering/design-system-audit.md`. Treat AutoWhisper as being in Phase 0 until these artifacts are verified and committed.

### Consequences

- Future runs have durable state and can continue the phased loop instead of re-orienting from scratch.
- Feature work is intentionally deferred until Phase 0 orientation is complete.
- Product claims and competitor comparisons remain unverified until Phase 1 research.

## ADR-0002 — Define current product hypothesis conservatively

Date: 2026-04-30T14:28:38Z

### Context

The README and code indicate an Ubuntu/X11 local dictation app, while the competitive prompt frames the category broadly against Wispr Flow, Superwhisper, MacWhisper, and Aqua Voice. The code contains macOS and Windows platform files, but they are not equivalent product-ready implementations.

### Decision

Use this current hypothesis for Phase 0: AutoWhisper is an offline, local-first dictation tool for Ubuntu/X11 users who want hotkey-triggered speech-to-text inserted directly into any desktop app, optimized around local Whisper/Distil-Whisper models, optional NVIDIA/CUDA acceleration, low-latency push-to-talk ergonomics, and simple systemd/PPA distribution.

### Consequences

- Phase 1 competitor research must test whether the target should remain Ubuntu/Linux-first or shift toward Mac/mainstream dictation.
- Marketing must not overclaim cross-platform support or “fastest” status before evidence exists.
- Engineering should preserve Linux stability while evaluating whether platform expansion is strategic.

## ADR-0003 — Use local preflight before pushing AutoWhisper work

Date: 2026-04-30T14:28:38Z

### Context

Channa requested use of `/autoplan`, `/ship`, and preflight checks, and specifically does not want GitHub CI run for every commit. GitHub CI should be reserved for major releases or deliberate remote validation.

### Decision

For AutoWhisper, prefer local verification before push. Use gstack-style `/autoplan` review planning and `/ship`/preflight semantics where available. Commit locally after verification; push only when the slice is intentionally ready for remote CI or release validation.

### Consequences

- Future runs should avoid casual pushes after each local commit.
- Local test/build commands and independent reviews become the default quality gate.
- GitHub Actions remain a release/major-change validation backstop rather than the primary feedback loop.

## ADR-0004 — Accept design-system direction and treat macOS as first-class target

Date: 2026-04-30T15:36:03Z

### Context

Channa provided an AutoWhisper design-system and UI concept package, then clarified that the goal is to build the best product in this category from the core local-first mission and to add a macOS version. The current production settings UI is embedded static HTML/CSS/JS served by the C++ app. The repo already contains macOS platform placeholders, but they do not implement global hotkeys, text insertion, or menu bar behavior.

### Decision

Adopt the uploaded paper-light, engineered design direction as the canonical design input. Port it into production incrementally without adding a frontend framework. Treat macOS as a first-class strategic product target while preserving Linux/X11 stability and avoiding claims that macOS is product-ready before implementation and verification.

### Consequences

- `design/` now preserves the source concept and documents the design system.
- `src/settings/web/` remains framework-free and schema-driven.
- `engineering/macos-roadmap.md` defines macOS as a deliberate platform program, not a placeholder promise.
- Future product claims about speed, accuracy, privacy superiority, or platform readiness still require benchmarks or implementation evidence.

## ADR-0005 — Keep vendored fmt/spdlog while unblocking AppleClang 21 audit builds

Date: 2026-04-30T17:03:00Z

### Context

Phase 0 audit build on macOS arm64 with AppleClang 21 failed inside vendored spdlog 1.14.1 / bundled fmt 10.2.1. AppleClang rejected fmt's consteval compile-time format checking with `call to consteval function ... is not a constant expression`. Replacing bundled fmt with a Homebrew/system fmt would add dependency drift and violate the current vendored dependency model.

### Decision

For AppleClang 21+ on macOS only, define `FMT_CONSTEVAL=` on the vendored spdlog targets after `add_subdirectory(deps/spdlog)`. Keep `SPDLOG_FMT_EXTERNAL=OFF`, keep bundled fmt, and avoid changing Linux behavior or adding runtime dependencies.

### Consequences

- macOS audit builds pass with the current vendored dependency set.
- fmt's consteval keyword path is disabled only for affected AppleClang builds; runtime formatting and dependency topology are unchanged.
- This should be revisited when spdlog/fmt submodules are intentionally upgraded.

## ADR-0006 — Build iOS as a native foreground app, not a desktop behavior clone

Date: 2026-05-01T01:30:49Z

### Context

Channa asked to build the iOS version of AutoWhisper. The current repo is a desktop-first C++ daemon/CLI app with Linux/X11 as the working product path and macOS platform files still placeholder-level. Read-only iOS scouts found no first-party iOS app target, no Xcode project, and no iOS SDK on the current machine. iOS does not allow third-party apps to provide global hotkeys, a menu-bar daemon, or arbitrary text injection into other apps.

### Decision

Treat iOS as a native sibling product surface. The first honest iOS product loop is foreground app → microphone permission → tap to record → local `whisper.cpp` transcription → transcript display → copy/share. Start with a Foundation-only Swift package under `ios/` so product defaults, permission/capability messaging, model catalog policy, and record/transcribe workflow seams are locally verifiable before full Xcode/iOS SDK installation.

### Consequences

- AutoWhisper must not market iOS as supporting desktop global hotkeys or arbitrary app text injection.
- The first iOS model catalog should prefer small bundled models such as `tiny.en`, not the large desktop default.
- SwiftUI, AVFoundation, simulator/device builds, signing, and App Store privacy review are next phases once full Xcode and iOS SDKs are available.
- Existing Linux desktop behavior remains preserved while iOS foundations are added in a separate `ios/` subtree.


## ADR-0007 — Desktop platform proof distinguishes buildability from runtime readiness

Date: 2026-05-01T02:30:00Z

### Context

Channa asked for macOS and Windows next, including Docker local end-to-end checks and screenshots. Docker on macOS runs Linux containers, not macOS containers, and standard Docker on this host cannot execute native Windows GUI/runtime flows. The repo can, however, prove macOS host-local build/test behavior and Windows MinGW cross-build artifact generation.

### Decision

Treat this slice as a desktop-platform foundation gate: macOS is validated host-locally with native CMake/CTest and settings UI screenshots; Docker validates Linux focused tests and Windows x86_64 MinGW cross-build artifacts only. Do not claim Windows runtime end-to-end until a Windows host/VM or proven Wine-capable runner executes the Windows `.exe` tests.

### Consequences

- Platform diagnostics explicitly expose ready/partial/placeholder/unsupported feature states.
- `engineering/desktop-platform-foundation.md` documents Docker limits and avoids claiming macOS containers or Windows runtime execution.
- Cross-compiled tests are not registered unless `CMAKE_CROSSCOMPILING_EMULATOR` is present.


## ADR-0008 — Production-readiness plan and decisions D1–D7 ratified

Date: 2026-06-12

### Context

Channa requested a plan to take AutoWhisper to production level and public readiness on
macOS, Windows, and Linux, with complete sign-on and feature parity with Wispr Flow. The
plan was authored as `docs/plans/2026-06-12-production-readiness-plan.md` (goals G1–G8,
milestones M1–M7, decision log D1–D7) and Channa signed off on the strategic calls.

### Decision

The plan is ratified and supersedes the provisional milestones in `.hermes/roadmap.md`.
Decisions adopted as recommended:

- D1: local dictation stays free/unlimited; Pro = sync + hosted AI polish; open-core boundary at the client/backend line.
- D2: managed identity provider (Auth0/Clerk class) with Google, Apple, Microsoft, email and a SAML path.
- D3: hybrid LLM strategy — bundled local small model by default, BYO cloud key opt-in.
- D4: Windows code signing via Azure Trusted Signing.
- D5: Sentry Native crash reporting, strictly opt-in.
- D6: trademark review of the "AutoWhisper" name before public launch (M6 gate).
- D7: transcription history off by default.

### Consequences

- M1 ("Truth & foundations") execution starts immediately: X11 resume-deadlock fix,
  multilingual model catalog with auto language detection, CI matrix (macOS/Windows),
  settings-server hardening, model checksum verification, benchmark harness.
- Parity is experience parity: the offline/local-first/open-source/Linux differentiators
  are preserved in every milestone.

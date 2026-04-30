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

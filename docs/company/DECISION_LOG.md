# AutoWhisper Decision Log

Last updated: 2026-05-03

## 2026-05-03 — Establish product operating system

Context: The repo had Hermes campaign state but not a root `AGENTS.md` or the required product operating docs under `docs/company`, `docs/research`, `docs/product`, `docs/design`, and `docs/engineering`.

Decision: Add `AGENTS.md` as the repo-level operating contract and initialize the required docs as living source-of-truth artifacts. Keep global identity in `~/.hermes/SOUL.md`, outside the repo.

Consequences:
- Future work should begin by reading `AGENTS.md`.
- Product, research, design, and engineering truth belongs in repo docs, not long-term memory.
- Feature work should be prioritized through the gap ledger and roadmap rather than ad hoc prompts.

## 2026-05-03 — Keep iOS claims constrained

Context: The iOS app shell can record foreground audio and has a placeholder transcription seam, but real whisper.cpp transcription and physical-device proof remain absent.

Decision: Treat iOS as foundation/beta only until real local transcription and runtime proof exist.

Consequences: Marketing, README, and roadmap language must not imply TestFlight/App Store readiness or real iOS Whisper output.

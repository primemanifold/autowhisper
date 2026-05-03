# AutoWhisper Roadmap

Last updated: 2026-05-03

Scores are 1-5. Higher is better except risk/cost are inverse-scored per `AGENTS.md`.

| Rank | PR-sized improvement | Impact | Confidence | Effort | Strategic leverage | Why now |
|---:|---|---:|---:|---:|---:|---|
| 1 | Add iOS/macOS recording interruption and background-state reconciliation | 4 | 4 | 3 | 4 | Prevents UI from lying when the recorder is stopped by the OS. |
| 2 | Add lightweight transcription benchmark harness and baseline corpus | 5 | 3 | 2 | 5 | Turns speed/quality claims into evidence. |
| 3 | Harden macOS first-run model download and permission validation around public artifact | 5 | 4 | 3 | 5 | Directly improves non-CLI activation and trust. |
| 4 | Create source-linked competitor research refresh and gap triage cadence | 3 | 4 | 2 | 4 | Keeps roadmap grounded without copying competitors. |
| 5 | Replace static iOS string tests with Swift/XCTest seams for recorder/transcriber state | 4 | 3 | 2 | 4 | Reduces false confidence in iOS readiness. |

## Phase gates

### Phase A — Foundation truth

- Required operating docs exist.
- Readiness matrix and validation commands stay current.
- CI proves Linux static/native checks and iOS app/widget builds.

### Phase B — Activation hardening

- macOS public artifact validates from downloaded ZIP.
- first-run model and permissions UX is reliable.
- iOS recording state handles permission, transition, interruption, and background semantics honestly.

### Phase C — Quality measurement

- Add benchmark harness for latency, output quality, and cleanup behavior.
- Publish only measured performance claims.

### Phase D — Differentiated workflows

- Add transformations, custom vocabulary, contextual rewrite, or developer workflow features only after activation and benchmarks are stable.

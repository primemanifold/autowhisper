# Gap Ledger

Last updated: 2026-05-03

## Open gaps

### Gap: iOS recorder can still drift on OS interruptions/backgrounding

Competitor / user need: Reliable mobile recording state.

Evidence: `ios/AutoWhisperApp/IOSAudioRecorder.swift` uses `AVAudioRecorderDelegate` but interruption/background reconciliation is not yet implemented; `ContentView.swift` recently added transition serialization but not interruption handling.

Severity: High

Opportunity type: Trust / Reliability

Proposed fix: Observe AVAudioSession interruptions and scene phase, implement delegate callbacks, reconcile UI state, and show clear interrupted/failed copy.

Estimated effort: Medium

Risks: iOS lifecycle complexity and static tests may not catch runtime behavior.

Metric: No UI state says recording when recorder is stopped by interruption/background.

Status: Open

Owner: Engineering

Last updated: 2026-05-03

### Gap: No benchmark harness for latency or transcription quality

Competitor / user need: Credible speed and accuracy claims.

Evidence: README includes model speed table, but no repo-level benchmark corpus or reproducible benchmark command is documented in `docs/product/METRICS.md` or `docs/engineering/PERFORMANCE_BASELINE.md` yet.

Severity: High

Opportunity type: Performance / Trust

Proposed fix: Add a lightweight benchmark suite with short audio fixtures, latency timings, output quality checks, and environment capture.

Estimated effort: Medium

Risks: Benchmarks can be misleading if hardware/model state is not captured.

Metric: Repeatable command outputs time to final transcript and quality notes.

Status: Open

Owner: Engineering

Last updated: 2026-05-03

### Gap: Product docs are newly initialized and need steady maintenance

Competitor / user need: Clear positioning and roadmap.

Evidence: `AGENTS.md` now requires docs under `docs/company`, `docs/research`, `docs/product`, `docs/design`, and `docs/engineering`; this run initializes them, but many are first-pass.

Severity: Medium

Opportunity type: Product / Process

Proposed fix: Update these docs after each meaningful PR and run weekly competitor/CEO reviews.

Estimated effort: Low recurring

Risks: Documentation can become a junk drawer if not pruned.

Metric: Each roadmap item links to evidence and stale gaps are closed/merged.

Status: Open

Owner: CEO / Operator

Last updated: 2026-05-03

### Gap: First-run activation proof differs by platform

Competitor / user need: A non-CLI user should reach first successful dictation quickly.

Evidence: README and platform readiness docs distinguish Linux, macOS, Windows, and iOS; public macOS artifact validation and iOS CI were recently improved but runtime E2E proof remains platform-specific.

Severity: High

Opportunity type: Activation / Trust

Proposed fix: Maintain platform-specific first-run smoke scripts and evidence artifacts.

Estimated effort: Medium

Risks: Overclaiming readiness if a build passes but permissions or insertion fail.

Metric: Each platform has build/package/install/runtime/E2E columns with dates and commands.

Status: Open

Owner: Engineering

Last updated: 2026-05-03

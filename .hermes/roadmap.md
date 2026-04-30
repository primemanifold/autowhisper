# AutoWhisper Roadmap

Status: Phase 0 bootstrap. This roadmap is intentionally provisional until Phase 1 benchmark and Phase 2 positioning are complete.

## Current phase gate

### Phase 0 — Orient

Definition of done:

- [x] `.hermes/state.md` exists.
- [x] `.hermes/decisions.md` exists.
- [x] `engineering/architecture.md` exists.
- [x] `engineering/design-system-audit.md` exists.
- [x] One-paragraph current product hypothesis exists.
- [x] Phase 0 files verified locally.
- [x] Phase 0 bootstrap committed.

## Next provisional milestones

These are not Phase 3 roadmap commitments. They are the immediate phase gates implied by the campaign prompt.

### Milestone 0.1 — Complete Phase 0 bootstrap

Success metric:

- Required state and engineering docs exist, are grounded in repo evidence, and are committed.

Definition of done:

- `engineering/architecture.md` accurately maps current repo structure and runtime flow.
- `engineering/design-system-audit.md` scores design maturity and concrete gaps.
- `.hermes/state.md` ends with a structured run update.
- `.hermes/decisions.md` logs initialization and current product hypothesis.
- Local preflight for documentation-only change passes: `git diff --check` and a reviewer/read-only sanity check.

### Milestone 1.0 — Map competitive landscape

Success metric:

- `research/benchmark.md` quantifies us vs the selected closest competitor with sourced evidence and weighted scoring.

Definition of done:

- Top 5 competitors selected with rationale.
- Teardowns exist under `research/competitors/`.
- Closest competitor named and justified.
- All claims have URL/source evidence or are marked `[UNVERIFIED]` / `[ASSUMPTION]`.

### Milestone 2.0 — Ratify positioning

Success metric:

- `marketing/positioning.md` is ratified by CEO hat and constrains roadmap priorities.

Definition of done:

- ICP, JTBD, positioning statement, message house, and naming check exist.
- CEO decision is logged in `.hermes/decisions.md`.
- Positioning explicitly resolves whether AutoWhisper remains Linux-first, local-first, or expands target platform/category.

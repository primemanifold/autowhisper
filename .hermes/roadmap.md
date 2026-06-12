# AutoWhisper Roadmap

Status: Phase 0 bootstrap. This roadmap is intentionally provisional until Phase 1 benchmark and Phase 2 positioning are complete.

> **2026-06-12:** A full production-readiness and Wispr Flow parity plan (goals G1–G8,
> milestones M1–M7 through v1.0, decision log D1–D7) is proposed in
> [`docs/plans/2026-06-12-production-readiness-plan.md`](../docs/plans/2026-06-12-production-readiness-plan.md).
> Once ratified, it supersedes the provisional milestones below.

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


### Milestone 0.2 — Design-system intake and first settings slice

Status: locally implemented, pending push.

Success metric:

- The uploaded design concept is preserved and the production settings UI has a first framework-free implementation slice that reflects the design direction.

Definition of done:

- [x] Source concept preserved under `design/source-concept/`.
- [x] Design-system intake documented in `design/README.md` and `design/design-system.md`.
- [x] Production settings UI uses `--aw-` tokens and intent navigation.
- [x] Settings UI still renders from `/api/schema`, `/api/config`, and `/api/defaults`.
- [x] Static regression tests prevent React/Babel/Tailwind adoption in production settings assets.
- [x] Advanced pane duplicate-control behavior verified with pane-scoped IDs and synced `data-config-key` controls.

### Milestone M0 — macOS build truth

Success metric:

- AutoWhisper has a verified macOS build and an honest feature/readiness matrix.

Definition of done:

- Current macOS placeholders are replaced or explicitly guarded.
- Homebrew/Xcode prerequisites are documented.
- Menu bar, hotkey, output insertion, permissions, and packaging gaps are tracked as implementation work.
- No marketing surface claims macOS readiness before local verification.

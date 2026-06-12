# Motion & platform design research (2026-06-12)

Per-system conventions gathered before adding animation to any AutoWhisper
surface. Sources fetched June 2026.

## Shared motion principles (all systems)

- User-initiated interactions get **ease-out** (fast start = perceived
  responsiveness, gentle settle). [web.dev easing guide,
  animations.dev easing blueprint]
- Micro-interactions live in **120–200 ms**; entrances up to ~350 ms;
  anything longer must be earning attention. [2026 micro-interaction
  surveys: bricxlabs, primotech]
- **`prefers-reduced-motion` is non-negotiable** and supported by every
  major browser; reduced means minimize or eliminate, not just shorten.
  [MDN, W3C WCAG C39]
- 2026 expectation: **one harmonized motion system across desktop, mobile,
  and watch** — same easing family and rhythm, platform-tuned amplitude.

## Desktop settings UI (Linux/macOS/Windows web surface)

- A settings app is an instrument, not a stage: animate **state changes**
  (pane switch, status arrival), never idle chrome.
- Hover/press: 120 ms; pane transitions: ~180 ms fade + small rise.
- Apple HIG: "avoid adding motion to interactions that occur frequently";
  the system already animates standard controls. Applies directly — no
  per-field animation.

## Landing page (web)

- **CSS scroll-driven animations** (`animation-timeline: view()`) are
  effectively universal in 2026: Chrome/Edge 115+, Safari 26, Opera 101+;
  Firefox partial behind a flag → ship inside `@supports` as progressive
  enhancement so Firefox simply sees the static page. [caniuse, MDN,
  Chrome dev blog]
- Pattern: one-time hero entrance on load; below-the-fold sections reveal
  on scroll entry (fade + ≤24 px rise, finish by 50–60% of entry). No
  parallax, no pinned scenes — those are the new slop.
- Everything wrapped in `@media (prefers-reduced-motion: no-preference)`.

## iOS (Apple HIG — Motion)

- "Prefer quick, precise animations"; brevity conveys information better
  than spectacle. Springs are the system feel (iOS 26 fluid morphs), but
  subtle: feedback, status, continuity — not decoration.
- Reduced Motion is an App Store accessibility evaluation criterion:
  parallax/depth/multi-axis effects must disable.
- For the AutoWhisper comp: a breathing record affordance (status: ready)
  and a blinking caret (status: live transcript) are the only two moving
  elements. Both are status, neither is decoration.

## Android (Material 3 Expressive — Motion physics)

- M3 replaced duration/easing tokens with a **physics/spring system**:
  spatial springs (position/size — may overshoot) vs **effects springs
  (color/opacity — must not overshoot)**.
- Two schemes: *expressive* (low damping, visible bounce; hero moments)
  and *standard*. 21 Compose components ship on the physics system.
- CSS approximation for comps and the future web surface: spatial moves
  may use a slight-overshoot bezier (`cubic-bezier(.34, 1.3, .45, 1)`);
  opacity/color stay on plain ease-out. Encoded as `--aw-ease-spring`.

## watchOS (Apple HIG — Designing for watchOS)

- Glanceable: interactions last seconds; crucial information must be
  visible immediately, stationary or in motion.
- Motion budget is the smallest of any platform: one slow breathing cue on
  the single primary control, nothing else. Always-on display implies the
  idle state must also read perfectly as a still.

## Resulting motion tokens (added to the design system)

| Token | Value | Use |
|---|---|---|
| `--aw-dur-1` | 120 ms | hover, press |
| `--aw-dur-2` | 180 ms | pane/status state changes |
| `--aw-dur-3` | 320 ms | entrances, reveals |
| `--aw-ease-out` | `cubic-bezier(.16, 1, .3, 1)` | default, all systems |
| `--aw-ease-spring` | `cubic-bezier(.34, 1.3, .45, 1)` | Android spatial only |
| breathing cue | 2.8 s ease-in-out loop | record affordance, mobile/watch |

Reduced motion: every surface carries a `prefers-reduced-motion: reduce`
block that disables transitions and animations wholesale.

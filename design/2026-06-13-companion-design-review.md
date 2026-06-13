# Companion overlay — deep design review and the pantheon of five

Date: 2026-06-13. Scope: the floating companion (`src/avatar/`) as an
overlay agent — reviewed against the desktop-companion ecosystems the
category learned from, with the character system expanded from three
forms to a cast of five. Method: code review of the renderer, state
machine, and shells; prior-art survey; pixel evidence rendered from the
production rasterizer (5 characters x 6 states, light and dark plates);
runtime smoke under Xvfb.

## Prior art studied

- **OpenPets** (github.com/alvinunreal/openpets) — desktop pets for AI
  coding agents. Patterns worth holding: pets are *packs* defined by
  manifest + animation frames; agent activity maps to a small reaction
  vocabulary (thinking/editing/testing/success/error); speech bubbles
  pass through **strict local redaction** (no paths, URLs, secrets, code);
  local-only IPC with per-run tokens; a gallery to preview and assign
  characters.
- **Shimeji / Shijima** — the character lesson: identity is carried by
  silhouette and idle behavior, not palette. Every shimeji is readable
  as itself in a 64px greyscale thumbnail. The anti-lesson: wandering,
  window-climbing pets are charming for minutes and intrusive for hours.
- **Clippy** (original + felixrieseberg/clippy revival) — instant
  silhouette recognition, and the canonical warning: an assistant that
  interrupts uninvited becomes the most hated character in software.
  Clippy's failure was *behavioral*, not visual.
- **Wispr Flow** — the floating button as a complete mouse-only control
  surface; already the companion's stated model.

## What the companion already gets right (verified in code)

1. **One renderer, three shells** — all motion math and pixels in shared,
   unit-tested code; platform layers are blitters. This is stronger than
   any of the surveyed projects, which fork per-platform behavior.
2. **Latch rules in the state machine** (Summoned 0.45s, Writing >=1.4s)
   — instant daemon transitions still *read* as events. OpenPets has no
   equivalent; fast agent events visibly stutter there.
3. **Truthful presence** — the halo follows the real microphone peak;
   the companion is the product's status made visible, never decoration.
   This is the privacy story: a *live, honest* listening indicator.
4. **Calm at rest** — idle is a quiet breathing mark with no caption;
   Clippy's behavioral failure is structurally avoided (the companion
   never initiates, only reflects).
5. **Opt-in, draggable, click-to-dictate** — non-activating panels
   (NSPanel non-activating, X11 override-redirect, WS_EX_NOACTIVATE
   tool window) mean it never steals focus; click/drag discrimination by
   movement threshold.
6. **Reduced-motion stills** — every state reads as a still frame (the
   watch always-on principle).

## Findings

**F1 — Character identity was palette-only (fixed this pass).** The three
forms shared one silhouette and differed by accent + tempo. By the
Shimeji/Clippy standard that is one character with three coats of paint.
Resolution: each spirit now carries a **signature** — a single ink-drawn
geometric tell rendered with the mark — so every form is recognizable in
silhouette alone, at 64px, in greyscale. Signatures are ink (never
accent): identity lives in shape; color keeps carrying meaning only.

**F2 — Cast expanded 3 → 5 (this pass).** The pantheon:

| id | form | epithet | signature | accent | tempo |
|---|---|---|---|---|---|
| `echo` | Echo | the one who answers | none — the pure sigil | record red | 1.00 |
| `hermes` | Hermes | the swift herald | wings: two strokes sweeping up from the dashes | message blue | 0.85 |
| `mnemosyne` | Mnemosyne | keeper of memory | a second ring held within the ring | amber | 1.20 |
| `kalliope` | Kalliope | the beautiful-voiced | a crown of three muse-stars above the ring | violet | 0.95 |
| `morpheus` | Morpheus | the shape of dreams | dream motes drifting up and away | teal | 1.35 |

Casting logic stays in the product's register: spirits of voice, memory,
and message — Echo answers, Hermes carries, Mnemosyne keeps, Kalliope
gives eloquence, Morpheus drifts (the slowest tempo; for people who want
the companion barely awake). Tests pin the registry, pairwise-distinct
idle silhouettes at t=0 (where tempo cannot help), and that signature
geometry never contaminates the caption strip.

**F3 — Overlay gotcha inventory** (the checklist this category keeps
getting wrong), current status:

| gotcha | status |
|---|---|
| Focus stealing | OK — non-activating window styles on all three shells |
| Click vs drag | OK — movement threshold before a click becomes a move |
| Caption legibility on any wallpaper | OK — rim-backed plate + built-in 5x7 font |
| Reduced motion | OK by design (stills read); no explicit OS-preference plumbing yet — acceptable while opt-in |
| Privacy of overlay content | OK today (status words only). **Rule for the M4 speech-bubble layer: adopt OpenPets-style redaction — never render transcript text, paths, URLs, or secrets in the overlay.** |
| Screen-edge / multi-monitor clamping | OPEN — position is user-dragged and unclamped; a monitor unplug can strand the companion off-screen |
| DPI / scaling | PARTIAL — `avatar.size` (64–192) is manual; no per-monitor DPI awareness |
| Fullscreen apps | OPEN — topmost overlay over fullscreen games/video is unverified per platform |
| Idle power | PARTIAL — ~30fps tick even at idle; an idle frame-rate drop (e.g. 8fps while breathing) is cheap |

The three OPEN/PARTIAL rows are tracked as future work; none regress the
current opt-in posture.

## What we deliberately did not take

- **Wandering/window-climbing** (Shimeji): violates "calm at rest" and
  the instrument thesis. The companion floats where you put it.
- **Dynamic speech bubbles** (OpenPets): deferred to the M4
  conversational tier, and only with the redaction rule above.
- **Multiple simultaneous companions**: one spirit at a time; the
  pantheon is a wardrobe, not a zoo.

## If the cast grows past five

OpenPets' pack/manifest model is the right shape: a declarative character
description (id, epithet, accent, tempo, signature geometry) loaded at
runtime. Today five `AvatarCharacter` entries in tested C++ are simpler
and pixel-identical across platforms; revisit manifests only if community
characters become a goal.

## Verification record (2026-06-13)

- 180/180 ctest (registry, silhouette-distinctness, caption-strip
  cleanliness, plus the full prior suite) — Linux release build.
- `autowhisper avatar demo --character morpheus` full state script under
  Xvfb.
- Pantheon sheet (5 characters x Idle/Summoned/Listening/Thinking/
  Writing/Error) rendered from the production rasterizer over light and
  dark plates; all five silhouettes distinct at 112px and legible at
  thumbnail size.
- Settings UI picks up the new enum values from the schema with no UI
  change (schema-driven rendering).

# UI/UX audit — all platform surfaces (2026-06-12)

Method: run the real product UI, capture screenshots at desktop and mobile
viewports with headless Chromium, read the pixels, fix what the pixels show,
re-capture to verify. Mobile platforms (iOS, Android, watchOS) have no
shipping UI yet, so their pass produced device-accurate design comps on the
`--aw-` token system rather than claims about apps that don't exist.

Captures live in `site/assets/screenshots/` (real UI and concepts are
labeled; every concept image has "design preview" baked into the pixels so
it cannot be mistaken for a product capture out of context).

## Surface inventory

| Surface | Status | Audited via |
|---|---|---|
| Settings web UI (Linux/macOS/Windows, served by the app) | shipping | live capture, this audit |
| Linux tray (GTK/AppIndicator) | shipping | code + icon review only (needs desktop session for pixels) |
| macOS menu bar + SwiftUI first-run helper | shipping (0.7.1) | not auditable from this Linux host — needs a Mac pass |
| Windows tray/UI | M2 stubs | nothing to audit yet |
| Landing page (`site/`) | shipping | live review |
| iOS / Android / watchOS | no UI exists | design comps created (`design/mobile/`) |

## Findings

**F1 — Float noise in the settings UI (fixed).** Paste Delay rendered as
`0.05000000074505806`. The June 12 TOML fix didn't cover the JSON API path;
`config_to_json` widened float→double. All config floats now route through
`shortest_double()` (`src/config/config.cpp`), shared by TOML and JSON
serialization. Verified via `/api/config` and re-capture.

**F2 — Mobile-width layout blowout (fixed).** At ≤820px the tab nav's
`max-content` items widened the single grid track to ~1100px, pushing the
whole page past the viewport: clipped lede, clipped status banner,
half-visible Save button. Fix: `min-width: 0` on `.aw-sidebar` in the
breakpoint plus left-aligned action wrap. Verified at 393×852.

**F3 — Stale embedded assets (fixed).** Web assets embed at CMake configure
time with no dependency tracking, so editing `style.css` and running
`cmake --build` shipped the old UI — this audit hit it live. Fix:
`CMAKE_CONFIGURE_DEPENDS` on the three web assets in
`cmake/EmbedAssets.cmake`.

**F4 — Persistent transient status (recommendation).** "Loaded from the
local AutoWhisper settings server." stays on screen forever in success
green. Transient confirmations should auto-dismiss (or demote to the
sidebar status line) so green is reserved for *changes* in state. Deferred:
cosmetic, low risk.

**F5 — Desktop information scent (recommendation).** The Privacy pane is an
honest placeholder, but it reads as an empty promise until the
network/retention proof panel exists (tracked in `design/design-system.md`
"Still needed"). Keep the pane, land the proof panel with M4 history work.

**F6 — Two landing surfaces (decision needed).** This repo's `site/`
(deployed by `pages.yml`, now unblocked since the repo is public) and the
separate `primemanifold/autowhisper-landing` repo both exist. One should be
canonical before public launch (G8); recommend folding the external repo
into `site/` so screenshots, release CTAs, and tests live with the code.

**F7 — Landing page had zero product imagery (fixed).** Marketing claims
with no pixels. The page now shows the real settings UI (desktop + mobile
captures) and the three mobile design previews, each explicitly labeled.

## Mobile design direction (comps in `design/mobile/`)

All three honor ADR-0006: no claims of global hotkeys or cross-app
injection on platforms that forbid them.

- **iOS** (`ios.html`): foreground loop — hold to dictate, on-device
  whisper.cpp, transcript card with Copy/Share, opt-in recents list.
  Light `--aw-` paper theme, model chip states the on-device model.
- **Android** (`android.html`): same loop, Material-leaning shapes,
  multilingual auto-detect surfaced in the transcript label; the system
  voice-keyboard (IME) — Android's legitimate system-wide dictation path —
  is shown as an explicitly tagged "Planned" entry point.
- **watchOS** (`watchos.html`): companion surface. The watch records and
  hands off to the paired iPhone for transcription ("Hold · sends to
  iPhone"); last note echoes back. Dark theme per watch convention.

Implementation sequencing stays per the ratified plan: mobile apps are
post-1.0 (M7), building on the `ios/` Swift foundation. These comps are the
design contract for that work, not a shipping claim.

## Re-audit checklist for the next pass

- [ ] macOS: menu bar states, SwiftUI helper, permission prompts (needs Mac)
- [ ] Linux: tray icon legibility on light/dark panels, GNOME vs KDE
- [ ] F4 auto-dismissing status
- [ ] Dark-mode capture pass (tokens exist; verify real rendering)
- [ ] Keyboard-only walkthrough + screen-reader spot check (axe/VoiceOver)

## Round 2 — de-slop refactor (same day)

Owner feedback: the first pass read as generic AI output. Specific tells,
all removed:

- **Warm beige + terracotta palette** → pure neutral greys, color reserved
  for meaning (record red, focus blue, status green/amber). Light and dark
  are both first-class; dark is elevation-based (grey steps, OLED-leaning
  near-black), per 2026 dark-first practice.
- **CSS-drawn device hardware** (bezels, crowns, dynamic islands, fake
  status bars, wrist bands) → gone. Mobile comps are bare app screens,
  light and dark side by side, floating with a single soft shadow —
  press-asset style.
- **Numbered eyebrows, uppercase mono labels everywhere, decorative
  gradients, panel cards with "01/02/03"** → removed across the settings
  UI and the landing page. Mono now appears only on technical readouts.
- **Landing page** rebuilt: one giant statement ("Speak. It types."), one
  CTA pair, the real product screenshot center stage (art-directed
  light/dark swap via <picture>, still script-free), three terse fact
  columns, minimal previews.
- Buttons are ink pills (black on light, white on dark) matching the
  settings UI.

Static tests now also pin the dark-mode hero swap and the expanded asset
set. All captures regenerated.

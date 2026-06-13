# AutoWhisper Design System and UX Foundation

## Direction

AutoWhisper is a local dictation instrument. The product should feel precise, calm, private, and technically honest. It is not a chatbot, a cloud service, or a generic AI dashboard.

## Scene

A developer, writer, or operator uses AutoWhisper while focused in another desktop app. The settings UI is opened rarely, usually to pick a model, fix a microphone, change a hotkey, or diagnose why text did not appear. The interface should be light-first because the primary use case is daytime desktop configuration, with automatic dark support for system preference.

## Principles

1. **Local by construction**
   - Show local model, local audio path, and config path when the product has the data.
   - Avoid promises like “never leaves your machine” unless backed by an auditable network and telemetry panel.

2. **Legible mechanism**
   - Expose model, device, precision, hotkey, audio source, and output method in plain language.
   - Prefer “CPU, 8 threads” over vague states like “optimized.”

3. **Calm at rest**
   - Idle UI should be quiet. Recording and error states are the only moments that deserve strong signal color.

4. **Fast path first**
   - Most users need hotkey, model, audio input, and output method. Put those before raw daemon knobs.

5. **Operator escape hatch**
   - Advanced users can still see every schema-backed key without compromising the main flow.

6. **Evidence over marketing**
   - Mark speed, accuracy, and competitor comparisons as needing benchmarks until measured.

## Token system

Production CSS tokens live in `src/settings/web/style.css` and use the `--aw-` prefix.

2026-06-12 refactor: the palette moved from warm paper/terracotta to a pure
neutral system (zero-chroma greys) with dark mode as a first-class theme
built on elevation steps rather than borders. Color now carries meaning
only: red is reserved for the record action, blue for focus, green/amber
for status. Primary actions are ink-on-paper pills (black in light mode,
white in dark). Decorative gradients, numbered navigation, and uppercase
mono labels outside technical readouts were removed.

Second pass, same day: light and dark collapsed into a single token block
via `light-dark()` (one source of truth per token; `color-scheme` decides),
and the landing site (`site/styles.css`) and mobile comps
(`design/mobile/*.html`) now speak the same `--aw-` vocabulary. Tray icons
(`icons/*.svg`) were redrawn as geometric state glyphs — grey ring at rest,
solid red disc while recording, amber open arc while processing, red ring
with a bang on error — using sRGB approximations of the tokens, because
tray renderers cannot be assumed to support `oklch()`.

Core roles:

- `--aw-paper-0` through `--aw-paper-3`: canvas and surface stack
- `--aw-line`, `--aw-line-2`: borders and hairlines
- `--aw-ink-0` through `--aw-ink-4`: text hierarchy
- `--aw-signal`: sparse recording/action accent
- `--aw-ok`, `--aw-warn`, `--aw-err`, `--aw-info`: functional states
- `--aw-focus-ring`: keyboard focus affordance
- `--aw-font-sans`, `--aw-font-mono`: UI and technical readouts
- `--aw-radius-1/2/3`, `--aw-radius-pill`: corner system (themable)
- `--aw-shadow-1`: the single elevation shadow (themable)

## Appearance themes

The settings UI ships four appearances, switched from the sidebar and
persisted in `localStorage` (`aw-theme`), applied as
`html[data-theme="light|dark|dev"]` (absent = follow the OS):

1. **Auto** — `color-scheme: light dark`; the OS preference decides.
2. **Light / Dark** — force one scheme; same tokens, resolved by
   `color-scheme`, so the two can never drift.
3. **Dev** — the opt-in phosphor-terminal theme: green-on-black CRT
   palette (low-chroma green papers, phosphor-green ink ramp), mono type
   everywhere, square corners (`--aw-radius-* : 0`), hard offset shadows,
   static scanlines, and a faint glow on the page title. It is a pure
   token-override layer: no component, layout, or hierarchy changes, and
   color still carries meaning only (red records/errs, amber warns).
   Focus moves to amber so it stays visible on the green field.

`index.html` applies the stored choice in an inline head script before
first paint to avoid a theme flash. `light-dark()` and `data-theme`
require a 2024-baseline browser; the settings page opens in the user's
default browser, which the local sidecar already assumes is current.

## Information architecture

The settings UI groups current schema-backed sections into user intent:

1. Dictation behavior, maps to `[hotkeys]`
2. Model and performance, maps to `[model]`
3. Audio input, maps to `[audio]`
4. Output and insertion, maps to `[output]`
5. Privacy, currently a proof placeholder with no schema-backed settings
6. Feedback and tray, maps to `[feedback]` and `[tray]`
7. Diagnostics, maps to `[daemon]`
8. Advanced, shows every schema-backed key

## Component inventory

Implemented in the first slice:

- shell
- sidebar navigation
- top bar
- status region with `aria-live`
- dirty-state pill
- button variants
- settings cards
- schema field rows
- checkbox rows
- text, number, and select controls
- advisory note
- responsive mobile layout
- appearance switcher (Auto / Light / Dark / Dev)
- the cast: companion character picker (radio cards with silhouette
  glyphs, accent dots, epithets; schema-driven, falls back to a plain
  select for unknown values)
- dirty-row markers (amber inset, matching the unsaved pill)
- Ctrl/Cmd+S saves; closing with unsaved changes warns

Still needed:

- model availability card
- microphone test meter
- hotkey capture widget
- doctor results panel
- tray icon state assets
- onboarding checklist
- local-first proof panel backed by actual diagnostics
- benchmark and latency readout components

## Motion

Researched per platform in `design/research/2026-06-12-motion-platform-research.md`.

- Tokens: `--aw-dur-1` 120ms (hover/press), `--aw-dur-2` 180ms (state
  changes), `--aw-dur-3` 320ms (entrances); `--aw-ease-out` default,
  `--aw-ease-spring` reserved for Android spatial moves (M3 physics).
- Settings UI animates state changes only (pane switch, status arrival,
  press feedback) — never idle chrome, per Apple HIG "avoid motion on
  frequent interactions".
- Landing: one-time hero entrance + scroll-driven reveals via
  `animation-timeline: view()` inside `@supports`. Scroll reveals must not
  use `backwards` fill: an inactive view timeline (page fits the viewport)
  would freeze content invisible.
- Mobile/watch comps: a breathing record affordance and a live caret —
  status cues, not decoration. Watch gets the slowest loop (3.6s).
- Every surface ships a `prefers-reduced-motion: reduce` block that
  disables all animation; stills must read perfectly without motion.

## Accessibility requirements

2026-06-12 audit (Chromium + axe-core, all themes, 320–1440px, every
pane, long-content stress): text contrast now meets WCAG AA on every
surface — light `--aw-ink-3` darkened to L 0.525 (dark raised to 0.640),
config keys moved from `--aw-ink-4` to `--aw-ink-3` (`ink-4` is now
decorative-only, never text), and the light functional colors (`ok`,
`warn`, `err`) darkened so 10–12px badge text passes on their `-soft`
tints. Status/issue/description text uses `overflow-wrap: anywhere` so
unbroken error tokens cannot widen the page. Checkbox rows are `<label>`
wrappers (the whole row is one target) and checkboxes use ink, not
signal red — red stays reserved for the record action. Landing nav and
footer links carry invisible padding to reach 24px targets (WCAG 2.5.8).

- All controls must be keyboard reachable.
- Focus must remain visible via `--aw-focus-ring`.
- Loading, save, and error messages use `role="status"` with `aria-live="polite"`.
- Inputs must keep label association through `for` and `id`.
- Error text should be associated with invalid controls when validation becomes field-level.
- Respect `prefers-reduced-motion`.
- Avoid relying on color alone for status.

## Implementation stance

Use the React design concept as a visual reference only. Production runtime remains framework-free because the settings UI is embedded into a native C++ app and served by a local HTTP sidecar.

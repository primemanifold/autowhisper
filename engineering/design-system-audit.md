# AutoWhisper Design System Audit

Generated: 2026-04-30T14:28:38Z
Phase: 0 — Orient
Scope: current in-repo UI and user-facing surfaces at commit `2778945`

## Executive summary

AutoWhisper does not yet have a product-grade design system. It has a functional settings web form with native controls, a few SVG tray status icons, terminal output conventions, and default OS/browser styling. This is acceptable for an early Linux utility, but it is not a competitive foundation for a premium dictation product in the category of Wispr Flow, Superwhisper, MacWhisper, or Aqua Voice.

The immediate design priority is not visual polish for its own sake. The priority is to establish a durable substrate: tokens, components, interaction states, accessibility rules, copy conventions, and performance/error affordances that every future UI change must use.

## Audited surfaces

- Browser settings UI: `src/settings/web/index.html`, `style.css`, `app.js`.
- Tray status/menu: `src/tray/platform/tray_gtk.cpp`, `src/tray/tray_common.cpp`, `icons/*.svg`.
- CLI output and diagnostics: `src/cli/cli.cpp`, `src/doctor/doctor.cpp`, `src/models/models.cpp`.
- README and config docs: `README.md`, `config.toml`.

## Maturity scorecard

Scores use 1–5:

- 1 = absent or ad hoc.
- 2 = basic but not systematic.
- 3 = consistent enough for current scope.
- 4 = strong, documented, extensible.
- 5 = product-grade and competitive.

| Dimension | Score | Evidence | Gaps |
|---|---:|---|---|
| Design tokens | 1 | `style.css` defines only `:root { color-scheme; font-family }`; no named colors, spacing scale, radii scale, typography scale, motion tokens, z-index, or semantic states. | Create CSS custom properties for color, spacing, typography, radii, shadows, focus rings, and states. |
| Visual identity | 1 | No brand palette, mark usage, layout language, illustration/icon system, or product voice beyond utilitarian names. Tray icons are state glyphs only. | Define brand direction before premium marketing/UI work. |
| Typography | 2 | Uses system UI and monospace labels; limited sizing in CSS. | Establish type scale, label/help/error styles, and readable hierarchy. |
| Layout and spacing | 2 | Settings UI has max-width, sections, grid rows, and sticky footer. | Replace raw values with spacing tokens; add responsive and dense/comfortable modes. |
| Component system | 2 | Form rows, buttons, inputs, sections are implicit DOM patterns in `app.js` and CSS. | Create named component patterns: field row, select, number input, text array input, status alert, footer actions, cards/sections. |
| Theming | 2 | Browser honors `color-scheme: light dark` and `Canvas` for footer background. | No explicit light/dark palette, contrast rules, or theme testing. |
| Accessibility | 2 | Native labels and inputs are present; buttons are native. | Missing visible focus system, `aria-live` for save/error status, structured error summaries, keyboard flow audit, reduced motion policy, screen reader copy. |
| Motion | 1 | No motion system. | Decide whether motion is needed; define reduced-motion default and status transitions if used. |
| Content design | 2 | Schema descriptions provide some help text; README is concise. | No message house, no ICP-specific language, no onboarding copy, no empty/error/loading copy system. |
| Error and status states | 2 | UI has status text with `.ok`/`.err`; CLI uses colored output. | Need consistent severity states, recoverability guidance, validation placement, and status persistence. |
| Performance affordances | 1 | No UI display for latency, model warmup, recording duration, processing state beyond tray icon. | Add product-facing latency/perf language after budgets are instrumented. |
| Platform consistency | 2 | Linux surfaces are coherent enough; non-Linux implementations are stubs. | Avoid implying cross-platform design until macOS/Windows UX exists. |
| Testability of UI design | 1 | C++ tests cover settings API, not browser visual/a11y behavior. | Add lightweight browser/UI checks after design tokens/components exist. |

Overall maturity: **1.75 / 5**.

## Current settings UI structure

The settings UI is schema-driven:

- `index.html` provides a header, config path, empty form, footer, save/reset buttons, and status span.
- `app.js` fetches `/api/schema`, `/api/config`, and `/api/defaults`, then renders each config section as a native form.
- `style.css` supplies basic page width, field grid, sections, footer, and status colors.

Strengths:

- Simple implementation.
- Native controls are accessible by default more often than custom controls.
- Schema-driven rendering keeps UI aligned to config definitions.
- Uses `textContent` for dynamic strings, avoiding obvious HTML injection in labels/descriptions.

Weaknesses:

- No design tokens, component names, or reusable primitives.
- No accessible live region for status updates.
- No field-level validation display before failed save.
- No progressive disclosure for advanced settings.
- No novice guidance for model/device/compute choices.
- No loading skeleton or offline/server failure recovery beyond disabling buttons.
- No brand personality.

## Tray and icon system

Tray state exists for idle, recording, processing, and error. The tray shows state and exposes menu actions such as open config and quit. Icons exist under `icons/` as SVG assets.

Strengths:

- Clear state model.
- Minimal surface area.
- Aligns with Linux desktop utility expectations.

Gaps:

- No documented icon style rules.
- No accessibility text audit for screen readers/system tray names.
- No consistency plan for future macOS menu bar or Windows tray.

## CLI and diagnostics UX

CLI output uses color and simple symbols for success/warn/fail. `doctor` gives checks and suggested fixes.

Strengths:

- Developer/operator friendly.
- Good fit for Linux-first install/diagnostic flows.

Gaps:

- No copy taxonomy for severity, fixability, or privacy/performance claims.
- `doctor --fix` can run package-manager/sudo-adjacent actions; future copy should be explicit about side effects.

## Accessibility gaps to close before premium UI work

1. Add `aria-live="polite"` or `role="status"` for save/reset/error status.
2. Add explicit focus-visible styling that meets contrast requirements.
3. Add field-level errors linked with `aria-describedby`.
4. Add keyboard-only acceptance checks for settings UI.
5. Define reduced-motion default before any animation is added.
6. Verify dark-mode contrast after explicit tokens are introduced.

## Design-system foundation recommendations

Before building competitor-parity features with UI impact, create a small design substrate:

1. `src/settings/web/tokens.css` or a documented token section in `style.css`:
   - semantic color tokens: background, surface, text, muted text, border, focus, success, warning, danger.
   - spacing scale: 4/8/12/16/24/32.
   - radii: small/medium/large.
   - type scale: body, label, heading, code, helper.

2. Component patterns:
   - `.aw-section`
   - `.aw-field`
   - `.aw-label`
   - `.aw-help`
   - `.aw-input`
   - `.aw-button`
   - `.aw-status`

3. Documentation:
   - `engineering/design-system-audit.md` stays as the audit.
   - Future `engineering/design-system.md` should define accepted tokens/components and rules.

4. Verification:
   - Add browser-based smoke checks once a repeatable UI test harness exists.
   - Until then, require manual/agent gstack browse QA for UI-changing milestones.

## CEO note for future phases

Do not compare AutoWhisper’s UI against Wispr Flow or Superwhisper until Phase 1 competitor evidence is collected. Current gaps above are internal evidence from the repo, not competitor-derived requirements.

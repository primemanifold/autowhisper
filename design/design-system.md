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

Core roles:

- `--aw-paper-0` through `--aw-paper-3`: canvas and surface stack
- `--aw-line`, `--aw-line-2`: borders and hairlines
- `--aw-ink-0` through `--aw-ink-4`: text hierarchy
- `--aw-signal`: sparse recording/action accent
- `--aw-ok`, `--aw-warn`, `--aw-err`, `--aw-info`: functional states
- `--aw-focus-ring`: keyboard focus affordance
- `--aw-font-sans`, `--aw-font-mono`: UI and technical readouts

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

Still needed:

- model availability card
- microphone test meter
- hotkey capture widget
- doctor results panel
- tray icon state assets
- onboarding checklist
- local-first proof panel backed by actual diagnostics
- benchmark and latency readout components

## Accessibility requirements

- All controls must be keyboard reachable.
- Focus must remain visible via `--aw-focus-ring`.
- Loading, save, and error messages use `role="status"` with `aria-live="polite"`.
- Inputs must keep label association through `for` and `id`.
- Error text should be associated with invalid controls when validation becomes field-level.
- Respect `prefers-reduced-motion`.
- Avoid relying on color alone for status.

## Implementation stance

Use the React design concept as a visual reference only. Production runtime remains framework-free because the settings UI is embedded into a native C++ app and served by a local HTTP sidecar.

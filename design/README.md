# AutoWhisper Design Intake

This directory contains the first design-system input for AutoWhisper and the engineering handoff derived from it.

## Source concept

The uploaded concept is preserved in `design/source-concept/` for traceability. It includes:

- `AutoWhisper Design System.html`
- `index.html`
- `tokens.css`
- React JSX artboards for foundations, components, IA, settings, prototype, surfaces, and tweak controls

The concept is a reference artifact, not production runtime code. Production settings UI remains plain HTML, CSS, and vanilla JavaScript in `src/settings/web/`.

## Current production slice

The first implemented slice ports the durable design decisions into the embedded settings UI:

- paper-light, engineered visual system
- AutoWhisper token names prefixed with `--aw-`
- intent-based settings navigation
- schema-driven form rendering preserved
- privacy panel as a local-first proof placeholder
- live status region for loading, saved, staged defaults, and error states
- unsaved changes tracking
- framework-free runtime

## Product thesis

AutoWhisper should feel like a well-tuned instrument next to the keyboard: quiet at rest, precise in motion, local by construction, and legible enough that power users can see exactly which model, device, input, and output path is active.

## Non-negotiables

- Do not add a frontend framework for the embedded settings UI.
- Do not make unbenchmarked claims about speed, accuracy, or privacy superiority.
- Keep Linux/X11 stable while platform expansion work begins.
- Treat macOS as a first-class strategic target, not a README promise.
- Preserve schema-driven settings so C++ config remains the source of truth.

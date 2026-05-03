# Component Inventory

Last updated: 2026-05-03

## Web settings UI

Evidence: `src/settings/web/`.

- Navigation panes with hash/deep-link support.
- Schema-driven form controls.
- Dirty-state/save feedback.
- Field-level errors and warnings.
- Diagnostics/status surfaces.

## macOS native surfaces

Evidence: `platform/macos/SettingsApp.swift`, `src/platform/macos/onboarding.mm`.

- Native settings/onboarding helper.
- Menu-bar/app-bundle affordances.
- Permission and model setup flows in progress.

## iOS surfaces

Evidence: `ios/AutoWhisperApp/ContentView.swift`, `ios/AutoWhisperWidget/AutoWhisperWidget.swift`.

- Primary record/stop/decode button.
- Permission state messaging.
- Placeholder transcript output.
- Widget/deep-link quick-record launcher.

## Gaps

- No unified component spec for recording state indicators across platforms.
- No documented loading/error/success state inventory for model downloads.
- No visual regression suite.

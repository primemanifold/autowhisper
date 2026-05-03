# UX Patterns

Last updated: 2026-05-03

## Patterns to preserve

- Push-to-talk should be obvious and recoverable.
- Settings should be schema-driven rather than hardcoded one-off forms.
- Permission errors should include direct remediation guidance.
- Platform-specific constraints should be surfaced before failure.

## Voice-specific patterns needed

- Recording transition guard to prevent double-start races.
- Interruption recovery when OS stops recording.
- Model-missing path that offers download/setup, not silent failure.
- Confidence/error feedback after transcription.
- Optional cleanup/transform preview before insertion for high-risk text.

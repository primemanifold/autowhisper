# Security and Privacy Review

Last updated: 2026-05-03

## Current privacy posture

AutoWhisper's strongest trust claim is local-first transcription for supported desktop paths. This must remain tied to implemented behavior and documented exceptions.

## Data involved

- Microphone audio.
- Temporary or recorded audio files.
- Transcript text.
- User configuration.
- Potential future contextual app data if contextual rewriting is added.

## Current principles

- Do not send audio/transcripts to third parties without explicit user action and documentation.
- Document where audio is stored and when it is deleted.
- Keep cloud cleanup/transformation features optional and clearly labeled if added.
- Avoid committing secrets, model credentials, signing keys, or tokens.

## Known review gaps

- Retention/deletion behavior needs a cross-platform audit.
- Optional cloud processing policy is not defined.
- iOS privacy manifest exists, but real Whisper model integration will require renewed review.
- Marketing privacy copy must remain tied to platform-specific evidence.

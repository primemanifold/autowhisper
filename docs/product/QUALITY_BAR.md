# Quality Bar

Last updated: 2026-05-03

## Voice workflow states

Every capture/insertion surface should clearly represent idle, listening, processing, inserting, copied/done, failed, permission blocked, microphone unavailable, offline, low confidence, and correction-detected states where relevant.

## Release-quality requirements

- A user can reach first successful dictation without reading source code.
- Permission failures include actionable recovery.
- The app never appears to be recording when the OS has stopped the recorder.
- Platform claims match validation evidence.
- Audio and transcripts are not sent to third parties without explicit disclosure and user intent.
- Performance claims map to a benchmark run.

## Current weak spots

- Benchmark harness absent.
- iOS interruption/background handling incomplete.
- Some platform readiness remains build-proof rather than runtime-proof.

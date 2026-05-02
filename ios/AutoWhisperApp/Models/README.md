# Bundled model drop point

This directory is the iOS app-bundle location for future GGML Whisper model files.

Expected filenames are declared in `AutoWhisperCore.ModelCatalog` and mirrored by `ModelResources.plist`:

- `ggml-tiny.en.bin`
- `ggml-base.en.bin`

The real model binaries are intentionally not committed in this PR slice. Until a future slice adds a real bundled model and native whisper.cpp inference, `IOSWhisperModelLocator` reports a clear missing-model state and the app keeps showing bridge-pending placeholder output.

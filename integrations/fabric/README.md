# AutoWhisper for Fabric

This standalone plugin makes AutoWhisper a local speech-to-text provider for
Fabric. It starts `autowhisper serve --stdio` as a private child process, keeps
one Whisper model warm, and routes Fabric's existing audio transcription calls
through the versioned local protocol. It adds no Fabric model tool and opens no
network listener.

The plugin expects an AutoWhisper build that includes local protocol v1 (see
AutoWhisper PR #24 or a later release).

## Install

Build and install AutoWhisper, then download the recommended model:

```bash
autowhisper model download distil-small.en
pip install ./integrations/fabric
fabric plugins enable autowhisper
fabric config set stt.provider autowhisper
```

Fabric Desktop's existing audio transcription endpoint automatically follows
`stt.provider`; no Desktop-specific provider switch is needed.

## Configure

Behavioral settings belong in Fabric's `config.yaml`, not `.env`:

```yaml
plugins:
  enabled:
    - autowhisper

stt:
  provider: autowhisper
  autowhisper:
    executable: autowhisper
    model: distil-small.en
    language: en
    device: auto
    startup_timeout_seconds: 180
    request_timeout_seconds: 600
```

Set `executable` to the full binary path when AutoWhisper is not on `PATH`, for
example `/Applications/AutoWhisper.app/Contents/MacOS/autowhisper` on macOS.
Use `config_path` to select a non-default AutoWhisper TOML configuration.

Changing the selected executable, config, device, or model replaces the child
process on the next transcription. Normal calls reuse the same process and
loaded model. Fabric owns the child lifecycle and stops it at process exit.

## Test

Run the provider tests with Fabric available on `PYTHONPATH`:

```bash
PYTHONPATH=/path/to/fabric:integrations/fabric/src \
  python -m unittest discover integrations/fabric/tests -v
```

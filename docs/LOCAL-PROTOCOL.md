# AutoWhisper Local Transcription Protocol v1

AutoWhisper exposes its reusable inference runtime through a parent-owned stdio
service. The service keeps one selected Whisper model loaded and processes
newline-delimited JSON requests serially. It does not open a TCP socket, expose
a LAN service, or require a shared secret.

## Start the service

```bash
autowhisper serve --stdio --model distil-small.en
```

Protocol responses are written to stdout. AutoWhisper, whisper.cpp, and GPU
diagnostics are written to stderr. A host must read both streams so a full
stderr pipe cannot block the child process.

The parent owns the service lifecycle. It should send `shutdown`, close stdin,
wait a short bounded grace period, then terminate the child process tree if it
does not exit. EOF on stdin also stops the service.

## Envelope

Every request is one compact JSON object followed by `\n`:

```json
{"protocol":"autowhisper.local","version":1,"id":"req-1","method":"health"}
```

Every response echoes the request ID and contains exactly one of `result` or
`error`:

```json
{"protocol":"autowhisper.local","version":1,"id":"req-1","result":{"status":"ready"}}
```

```json
{"protocol":"autowhisper.local","version":1,"id":"req-1","error":{"code":"method_not_found","message":"Unknown method: example","retryable":false}}
```

Request IDs are non-empty strings of at most 128 bytes. One input line is
bounded to 1 MiB. An incompatible protocol name or version fails closed.

## Methods

### `health`

Returns `{"status":"ready"}` after the configured model has loaded. A host
should call this immediately after spawning the process.

### `capabilities`

Returns the methods, supported file formats, normalized sample rate/channel
count, and input bounds. Hosts should treat the advertised values as runtime
truth instead of duplicating them.

### `transcribe_file`

```json
{"protocol":"autowhisper.local","version":1,"id":"req-2","method":"transcribe_file","params":{"path":"/absolute/audio.wav","language":"en","model":"distil-small.en"}}
```

`path` is required. `language` and `model` are optional. The model must match
the model loaded at process start; change models by replacing the child process.
Supported input currently follows the bundled miniaudio decoders: WAV, MP3,
and FLAC. Audio is decoded to 16-kHz mono float samples and bounded
to 60 minutes and 512 MiB.

The result uses `fabric.transcription` v1:

```json
{
  "schema": "fabric.transcription",
  "version": 1,
  "request_id": "req-2",
  "status": "completed",
  "text": "Ship the voice note workflow.",
  "provider": "autowhisper",
  "language": "en",
  "duration_ms": 1840,
  "processing_ms": 412,
  "model": "distil-small.en",
  "segments": [
    {"start_ms": 0, "end_ms": 1840, "text": "Ship the voice note workflow."}
  ],
  "warnings": []
}
```

`status` is `completed`, `no_speech`, `cancelled`, or `failed`. A failed result
contains `error: {code, message, retryable}` and an empty `text`. Consumers must
ignore unknown fields so v1 can gain additive metadata.

### `shutdown`

Returns `{"status":"stopping"}` and exits after flushing the response.

## One-shot command

The same runtime is available without a persistent child process:

```bash
autowhisper transcribe recording.wav --model distil-small.en --language en --json
```

This is useful for diagnostics and simple command integrations. Hosts that
transcribe repeatedly should use the stdio service to avoid loading the model
for every recording.

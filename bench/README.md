# AutoWhisper benchmark harness

The harness is the source of truth for every speed/accuracy claim made about
AutoWhisper (G2 in the production plan). Numbers in the README, site, or docs
must come from a run of this tool on stated hardware — never from vendor
claims or guesses.

## What it measures

Per model and device:

- **model load time** (seconds)
- **latency** — median wall-clock seconds to transcribe each utterance
  (default 3 runs per utterance)
- **RTF** — real-time factor, latency ÷ audio duration (lower is better;
  < 1.0 is faster than real time)
- **WER** — word error rate against the manifest reference transcripts
  (normalized: lowercased, punctuation stripped)

## Running

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DAUTOWHISPER_ENABLE_BENCH=ON
cmake --build build -j$(nproc) --target autowhisper_bench

./build/autowhisper model download distil-small.en   # or any catalog model
./build/autowhisper_bench --manifest bench/manifest.tsv \
    --model distil-small.en --device cpu --threads 4 --json bench-results.json
```

`--device cuda` benchmarks the GPU path on CUDA builds.

## Fixtures

`manifest.tsv` is tab-separated: `wav path<TAB>reference transcript`.
Relative paths resolve against the manifest location. The default manifest
uses the `jfk.wav` sample shipped with the whisper.cpp submodule; extend it
with longer and noisier material (e.g. LibriSpeech test-clean excerpts) for
publishable accuracy numbers — a single short utterance is a smoke fixture,
not a corpus.

## CI

`.github/workflows/nightly-bench.yml` runs the harness nightly on the CPU
path with the smoke fixture and uploads the JSON as an artifact, so latency
regressions show up as trends rather than user reports.

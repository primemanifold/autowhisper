# AutoWhisper

**Speak. It types.** Hold a key — or click the floating companion — and your words land at the cursor. Offline, local-first, open source.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="site/assets/screenshots/settings-desktop-dark.png">
  <img src="site/assets/screenshots/settings-desktop.png" alt="AutoWhisper settings — real product UI, light and dark" width="100%">
</picture>

Native C++ on [whisper.cpp](https://github.com/ggerganov/whisper.cpp). Your voice never leaves your machine: no cloud, no account, no telemetry. 100+ languages with auto-detection.

| Platform | Status |
|---|---|
| Linux (X11) | Production — PPA, systemd service |
| macOS 12+ | Beta — notarized `.app` ([v0.7.1 release](https://github.com/primemanifold/autowhisper/releases/latest)) |
| Windows 10/11 | Beta — hotkeys, text insertion, and the companion work; tray and installer in progress |

## Install

**Fastest:** download your OS's binary from the latest green [Actions run](https://github.com/primemanifold/autowhisper/actions/workflows/ci.yml) → Artifacts, then follow **[docs/INSTALL-UNSIGNED.md](docs/INSTALL-UNSIGNED.md)** (one-time unsigned-binary prompt on Windows/macOS; nothing to skip on Linux).

```bash
# Ubuntu / Debian
sudo add-apt-repository ppa:primemanifold/autowhisper
sudo apt update && sudo apt install autowhisper
```

**First run on any machine** — models aren't bundled (78 MB–3 GB). One command, once:

```bash
autowhisper model download distil-small.en   # recommended English model, SHA-256 verified
autowhisper doctor                            # check mic, GPU, permissions
autowhisper run                               # hold Shift+Super (Ctrl+Alt+Space on Windows), speak, release
```

No model yet? The binary tells you this exact command instead of crashing. Zero-setup proof of life: `autowhisper avatar demo`.

## The companion

Echo — the nymph who can only repeat your words — is an opt-in floating button in the Wispr Flow tradition: **click to dictate**, watch the halo follow your voice, see her caption her own state and write your words down.

![Echo, the floating dictation button — live capture from the Linux build](site/assets/screenshots/avatar-floating-button.gif)

```toml
[avatar]
enabled = true      # forms: echo, hermes, mnemosyne
```

## What it does

- **Push-to-talk or toggle** dictation into any focused app, with a cancel key.
- **Cleans transcripts as you speak**: filler words removed, personal dictionary (`"auto whisper => AutoWhisper"`), spoken "new line" / "new paragraph".
- **Local settings app** served by the binary itself (`autowhisper config ui`) — token-protected, framework-free, light and dark.
- **Measured, not claimed**: speed numbers come from the bundled benchmark harness (`bench/`). Reference: 11 s of audio in ~0.8 s on 4 CPU threads with `tiny.en`.

Mobile (iOS / Android / watchOS) exists as a [design preview](design/mobile/) only — not shipping apps.

## Build from source

```bash
sudo apt install cmake g++ pkg-config libx11-dev libxtst-dev libxext-dev \
  libxi-dev libxrandr-dev libgtk-3-dev libayatana-appindicator3-dev libpulse-dev   # Ubuntu
git clone --recurse-submodules https://github.com/primemanifold/autowhisper.git
cd autowhisper
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build          # 178 tests
```

macOS needs only Xcode CLT + CMake (`cmake -B build && cmake --build build`). Windows builds with MSVC or the MinGW toolchain file (`cmake/toolchains/`). CUDA: `-DAUTOWHISPER_ENABLE_CUDA=ON` (12.x).

**Building or contributing with an AI agent?** Read **[AGENTS.md](AGENTS.md)** — exact build/test/verify commands, repo conventions, and how the planning docs fit together.

## Bugs and requests

Open an issue with the [bug](.github/ISSUE_TEMPLATE/bug_report.yml) or [feature](.github/ISSUE_TEMPLATE/feature_request.yml) template. Using Claude or another agent? Prompt it like:

> Look at primemanifold/autowhisper and file a bug: when I do X on `<OS>`, Y happens instead of Z. Include the output of `autowhisper doctor`, the artifact/run ID I used, and the relevant source file if you can find it.

Maintainer agents triage issues against `docs/plans/2026-06-12-production-readiness-plan.md`; well-scoped reports like [#14](https://github.com/primemanifold/autowhisper/issues/14) get implemented fast.

## More

`CHANGELOG.md` · production plan in `docs/plans/` · design system in `design/` · benchmark methodology in `bench/` · settings UI at phone width and mobile design previews in [`site/assets/screenshots/`](site/assets/screenshots/)

Apache-2.0

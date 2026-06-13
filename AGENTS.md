# AGENTS.md — building and contributing with an AI agent

This file is the agent-facing contract for the repo. Humans welcome too.

## Build and test (Linux, the canonical dev loop)

```bash
sudo apt-get install -y cmake g++ pkg-config libx11-dev libxtst-dev \
  libxext-dev libxi-dev libxrandr-dev libgtk-3-dev \
  libayatana-appindicator3-dev libpulse-dev xvfb
git submodule update --init --recursive --depth 1
cmake -B build -DCMAKE_BUILD_TYPE=Release -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure     # C++ suite (incl. Xvfb hotkey tests)
python3 -m unittest discover tests/static -v   # web/site/bundle asset checks
node --check src/settings/web/app.js           # settings UI syntax
```

Other targets:

- **Windows (cross)**: `cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release` — the exe must stay self-contained (only system DLLs; check with `objdump -p | grep "DLL Name"`). Runtime-smoke it under Wine.
- **macOS**: no cross-compile exists; the `macos-14` CI job is the compile truth. Don't claim macOS runtime behavior without a Mac.
- **Bench**: `-DAUTOWHISPER_ENABLE_BENCH=ON`, then `./build/autowhisper_bench --manifest bench/manifest.tsv --model tiny.en --device cpu`.

## Verify like this repo verifies

The house rule is **evidence over assertion** (see ADR-0007): buildability,
runtime behavior, and marketing claims are three different things.

- Runtime-smoke UI work under Xvfb and capture pixels
  (`./build/autowhisper avatar demo` needs no model or microphone).
- Performance claims come from `bench/`, never from prose.
- A change isn't done until `ctest` + static suites are green and the CI
  matrix (linux / macos / windows-cross / windows-msvc) passes on push.

## Where things live

| What | Where |
|---|---|
| Product roadmap & milestone gates | `docs/plans/2026-06-12-production-readiness-plan.md` |
| Architecture decisions (ADRs) | `.hermes/decisions.md` |
| Run/state journal | `.hermes/state.md` (append a structured update per work session) |
| Design system & UI audits | `design/` |
| Companion (avatar) design | `design/avatar-companion.md` |
| Unsigned install / first-run guide | `docs/INSTALL-UNSIGNED.md` |

## Conventions

- Branch from `core`; CI runs on pushes to `core` and `claude/**`.
- Conventional-commit style subjects (`feat(scope): …`, `fix(scope): …`).
- New config keys flow through all five layers: `config.h` struct,
  `schema.cpp`, `config.cpp` load/save/validate, `settings/handlers.cpp`
  JSON, and the settings UI pane map in `src/settings/web/app.js` — plus
  tests. The schema is the single source the UI renders from.
- Platform code follows the per-platform-file pattern
  (`src/<module>/platform/<module>_{x11,win32,macos}.{cpp,mm}`) with shared
  logic in `<module>_common.cpp` / core files that the test suite covers.
- Honest capability reporting lives in `src/platform/capabilities.cpp`;
  update it (and its test) when a platform feature changes state.

## CI economy

GitHub honors `[skip ci]` in the head commit message of a push. Use it
only when the exact tree being pushed has already been proven green:
the merge commit of a branch whose tip just passed the full matrix, or
a docs/site-only change validated by the full local loop above. Never
use it on code changes that haven't run the matrix, and never on
release-prep commits — releases always get a full CI run.

## Releases

1. Bump `project(autowhisper VERSION x.y.z)` in `CMakeLists.txt`, add a
   `debian/changelog` entry and a `CHANGELOG.md` section — the
   `ppa-release` workflow hard-verifies tag ↔ CMake ↔ debian agreement.
2. Merge to `core` with the full CI matrix green.
3. Tag `vx.y.z` on `core`; publish a GitHub release and attach the CI
   artifacts (macOS releases additionally get Developer ID signing +
   notarization on a Mac, per `docs/MACOS.md`).

## Filing issues (humans and agents)

Use the templates in `.github/ISSUE_TEMPLATE/`. A good report names the
OS, the artifact or commit, what you did, what happened, what you expected,
and pastes `autowhisper doctor` output for runtime problems. Agents: search
existing issues first, one problem per issue, and link the source file you
suspect — issue #14 is the reference example of a report that went from
filed to implemented same-day.

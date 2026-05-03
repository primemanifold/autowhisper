# Claude Scout Synthesis — Cross-Platform Readiness

Here is the full synthesis memo.

---

## AutoWhisper Cross-Platform Readiness — Synthesis Memo

---

### 1. Final recommended implementation lane order

**Execute in this order. Each lane unlocks the next; no lane starts without Channa's go/no-go.**

| Order | Lane | Rationale |
|---|---|---|
| **1st** | **A — Matrix + validation commands + claim-boundary tests** | Zero code risk; purely additive docs + test files; closes the documentation drift risk that makes all other lanes unsafe. All five scouts independently recommended this first. |
| **2nd** | **B — README usage guide + media scaffolding** | User-facing docs-only; enables honest platform communication before any real features ship; no code changes. |
| **3rd** | **C — macOS/Linux validation script hardening** | Requires a small doctor flag (`--headless`) — the only code change in the batch. Depends on Lane A (matrix) being already honest. |
| **4th** | **D — Windows readiness boundary** | Depends on Lane A establishing `build_proven` vocabulary; minimal CMake/script work. |
| **5th** | **E — iOS physical-proof preparation** | Depends on Lanes A + B establishing honest baselines; requires Xcode + simulator access. |

---

### 2. Risk-adjusted scope for the first implementation batch (Lanes A + B only)

**What is in scope for this batch:**

- All deliverables are markdown files and Python test files. Zero changes to C++, Swift, CMake, or shell scripts.
- Platform statuses are drawn directly from scout findings — nothing is promoted beyond what scouts documented.

**Risk reductions applied:**

| Deferred item | Why deferred |
|---|---|
| `linux_docker_validation.sh` headless doctor extension | Requires adding `--headless` flag to `doctor_linux.cpp` — a code change; separate approval required |
| iOS `xcrun simctl` smoke commands | Require running Xcode + live simulator; document in matrix/commands now, execute in Lane E |
| Windows Wine CI step | Scout explicitly says not to add unless end-to-end Wine execution is verified |
| Two additional `test_ios_app_shell.py` assertions (iOS scout §5) | Already-passing test file; defer iOS test changes to Lane E to avoid touching a passing file |

---

### 3. Exact files to create/modify

**Create (new files):**

```
engineering/platform-readiness-matrix.md
engineering/platform-validation-commands.md
tests/static/test_platform_readiness_docs.py
tests/static/test_docs_claims.py
docs/usage-guide.md
docs/media/README.md
docs/media/manim/autowhisper-flow-plan.md
```

**Modify (existing files):**

```
README.md   — add macOS quick-start, iOS honest status, Windows caveat, Wayland Linux warning
```

**Do not touch:**

```
tests/static/test_ios_app_shell.py    (passing; additions deferred to Lane E)
tests/static/test_website_assets.py  (passing; no new claims to enforce yet)
.hermes/state.md                      (only the final integration lane may touch this)
site/index.html                       (accurate; no change needed this batch)
Any C++, Swift, CMake, or shell file
```

**Exact content targets per file:**

**`engineering/platform-readiness-matrix.md`** — one row per platform, columns matching PRD §7 D1. Use these scout-sourced statuses:

| Platform | source_build | package | public_artifact | install_firstrun | microphone | quick_launch | text_output | model_mgmt | local_transcription | runtime_e2e |
|---|---|---|---|---|---|---|---|---|---|---|
| macOS | `ready` | `ready` | `release_proven` | `partial` | `partial` | `partial` | `partial` | `partial` | `partial` | `partial` |
| Linux | `build_proven` | `packaged` | `unverified` | `partial` | `partial` | `partial` | `partial` | `partial` | `unverified` | `unverified` |
| Windows | `build_proven` | `build_proven` | `unverified` | `unverified` | `build_proven` | `build_proven` | `build_proven` | `unverified` | `unverified` | `unverified` |
| iOS | `ready` | `build_proven` | `planned` | `unverified` | `partial` | `partial` | `unverified` | `unverified` | `planned` | `unverified` |

Each cell must include a "what proves this" and "what this does not prove" note. Add a `next_milestone` column drawing from each scout's §3.

**`engineering/platform-validation-commands.md`** — exact commands from:
- macOS scout §4 (Steps 1–7, with the proves/does-not-prove table)
- Linux scout §4 (both Docker/static and X11 runtime sections)
- Windows scout §4a–§4d (cross-build, manual MinGW, optional Wine, Windows gold standard)
- iOS scout §4 (simulator build, generic device build, Swift package check)

Each command must state what it proves and explicitly what it does not prove.

**`tests/static/test_platform_readiness_docs.py`** — all six test functions T1–T6 from macOS scout §5, plus the Windows doc claim-boundary grep from Windows scout §5d converted to Python.

**`tests/static/test_docs_claims.py`** — all nine test methods from product scout §6, verbatim.

**`README.md`** — add these sections using the structure from product scout §2:
- macOS quick-start: download link to v0.7.1 ZIP, unzip → Applications, three permissions, default hotkey
- iOS: honest beta shell description (recording ready, transcription pending, no App Store), explicit "no local Whisper yet"
- Windows: one-liner status — `build_proven`, runtime unproven, coming soon
- Linux: add Wayland warning inline admonition (product scout and Linux scout both identified this as highest-risk silent failure)

**`docs/usage-guide.md`** — full outline from product scout §3, no placeholder screenshots yet (use `[Screenshot: TODO — see docs/media/README.md]` captions).

**`docs/media/README.md`** — inventory table from product scout §4, with capture automation commands as documentation, marked `TODO: capture`.

**`docs/media/manim/autowhisper-flow-plan.md`** — full Manim scene plan from product scout §5, including narrative arc, scene list, visual design notes, output path, and rendering command.

---

### 4. Tests to add/run

**New test files (must pass before commit):**

| File | Tests | What they guard |
|---|---|---|
| `tests/static/test_platform_readiness_docs.py` | T1: README macOS permission mentions; T2: version citations adjacent to artifact URLs; T3: macOS `runtime_e2e` not `ready`; T4: audio not promoted to `ready` without evidence; T5: no DMG claim without artifact; T6: install.sh dev-mode language | macOS documentation drift |
| `tests/static/test_docs_claims.py` | 9 tests from product scout §6: macOS artifact link, site version match, iOS no-transcription, iOS README bridge-pending, widget no-recording, Windows caveat, Linux PPA ref, no feature parity claim, local-first backing | Cross-platform overclaim prevention |

**Existing tests that must still pass after modifications:**

```
python3 -m unittest tests.static.test_macos_app_bundle_assets -v
python3 -m unittest tests.static.test_ios_app_shell -v
python3 -m unittest tests.static.test_website_assets -v
```

**Note on `test_docs_claims.py` setup risk:** The `setUp` method reads `docs/MACOS.md`. Verify this path exists before running — the scout referenced `docs/MACOS.md` but this may be at a different path. Adjust the path constant in the test if needed.

---

### 5. Local verification commands

Run these in order after Lane A + B implementation, before any commit:

```bash
# 1. Full static test suite — all must pass
python3 -m unittest discover -s tests/static -v

# 2. No whitespace or merge-marker issues
git diff --check

# 3. Vocabulary integrity — every cell in the matrix uses only PRD §6 states
python3 -c "
import re, sys
from pathlib import Path
valid = {'ready','partial','build_proven','packaged','release_proven','unsupported','unverified','planned'}
text = Path('engineering/platform-readiness-matrix.md').read_text()
# find all backtick-quoted states in table cells
found = set(re.findall(r'\`([a-z_]+)\`', text))
bad = found - valid - {'n/a','tbd'}
if bad: sys.exit(f'Invalid states in matrix: {bad}')
print('Matrix vocabulary OK')
"

# 4. No Windows runtime overclaims in any new or modified docs
grep -rn --include="*.md" \
  -e "Windows.*ready" \
  -e "Windows.*supported" \
  -e "Windows.*works on" \
  engineering/ docs/ README.md \
  | grep -v "build_proven\|unverified\|not yet\|coming soon\|deferred\|cross-build only"
# Expect: no output

# 5. No iOS transcription overclaims in any new or modified docs
grep -rn --include="*.md" \
  -e "transcribe locally" \
  -e "local whisper" \
  -e "real transcription" \
  ios/ docs/ README.md
# Expect: no output

# 6. macOS source build still passes (proves matrix 'ready' claims are accurate)
cmake -S . -B build-audit \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAUTOWHISPER_ENABLE_TESTS=ON
cmake --build build-audit -j$(sysctl -n hw.ncpu)
ctest --test-dir build-audit --output-on-failure

# 7. git status clean check
git status --short --branch
```

---

### 6. Stop conditions requiring Channa approval

Stop immediately and ask before proceeding if any of these is true:

| # | Condition |
|---|---|
| **S1** | Any implementation touch on C++, Swift, CMake, Python (except new test files), or shell scripts — this batch is docs + tests only |
| **S2** | Any platform status promoted above scout-documented levels (e.g., Windows above `build_proven`, iOS `local_transcription` above `planned`, macOS `runtime_e2e` above `partial`) |
| **S3** | Any edit to the macOS release download URL in `README.md` or `site/index.html` — the v0.7.1 link is currently correct; only change with explicit release-asset proof |
| **S4** | Any push to GitHub — commit locally only; Channa approves push separately |
| **S5** | Any start of Lane C (script hardening), Lane D (Windows), or Lane E (iOS device proof) — each needs a separate go/no-go |
| **S6** | Any edit to `tests/static/test_ios_app_shell.py` — this file is passing; iOS test additions are deferred to Lane E |
| **S7** | Any edit to `.hermes/state.md` — only the final integration lane may touch this |
| **S8** | Any new dependency added to `Package.swift`, `CMakeLists.txt`, `debian/control`, or `pyproject.toml` |
| **S9** | If `test_docs_claims.py` setUp fails because `docs/MACOS.md` is not at that path — stop and report the actual path before writing the test |
| **S10** | If any existing static test suite starts failing after README edits — do not suppress or skip; stop and report |

---

**Ready for Channa review.** The first batch is Lane A + B: seven new files, one README modification, zero code changes. All platform statuses are drawn directly from scout evidence with no promotions.

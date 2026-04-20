# Settings UI C++ Rewrite — Progress Handoff

**Last updated:** 2026-04-20 (All phases complete — Tasks 1-28)

## Status: COMPLETE

All 28 tasks finished. 82/82 tests pass in clean Release build.
End-to-end smoke test verified:
- `GET /api/{schema,defaults,config}` → 200 JSON
- `GET /`, `/style.css`, `/app.js` → embedded assets
- `PUT /api/config` → 400 on invalid enum (byte-exact error), 204 on valid save
- Single-instance coordination: second invocation reports existing port
- `SIGTERM` → clean shutdown via self-pipe watcher

Commit range for Phase 2–10: `b8f20d8` → `c6cb8fe` (21 commits).

---

**Historical handoff (archived):**

This document captures the in-flight state of the settings UI rewrite so that work can resume in a new session, with this tool or a different one (Copilot, Codex, another agent, manual).

## What this work is

Replace the Python `tools/autowhisper-settings` tool with a C++ `autowhisper config ui` subcommand, extract a shared config schema used by both `Config::validate()` and the UI, and fix the tray's broken "Open Config" handler.

## Source artifacts

- **Design doc:** `docs/plans/2026-04-19-settings-ui-cpp-rewrite-design.md` — approved after 8 review rounds. Commits `208aa4a` → `bfe5773`.
- **Implementation plan:** `docs/plans/2026-04-19-settings-ui-cpp-rewrite-plan.md` — 28 tasks across 10 phases. Commit `7a077a9`.
- **This progress doc:** you are here.

## Branch state

- Working branch: `next` (dev branch; PRs eventually target `core`).
- Main branch: `core`.
- `origin/next` is up to date with local through commit `7a077a9` (spec + plan).
- Local commits for Phase 1 (`2488d0b` → `7e1d4f1`) have **not been pushed yet**. Push when convenient.

## Completed tasks

| Phase | Task | Commits | Status |
|---|---|---|---|
| 1 | Task 1: cpp-httplib submodule | `2488d0b`, `48082ef` (code-quality fix relocated CMake wiring to `cmake/FetchDependencies.cmake`) | ✅ |
| 1 | Task 2: nlohmann-json submodule | `78b2405` | ✅ |
| 1 | Task 3: schema.h | `325daea` | ✅ |
| 1 | Task 4: schema key table + JSON | `7d74f87` | ✅ |
| 1 | Task 5: schema unit tests | `7e1d4f1` | ✅ |

**Test suite: 57 → 64 tests, 100% pass.** `cd build && ctest --output-on-failure` confirms.

**Build: clean on Ubuntu with CUDA enabled.** `cmake --build build -j$(nproc)`.

## Remaining tasks (23 of 28)

Execute in order. Each task has full text, code, test cases, and commands in the plan doc. The plan is authoritative; this list is just an index.

| Phase | Task | File focus | Notes |
|---|---|---|---|
| 2 | Task 6 | `src/config/config.cpp` | **Highest risk.** Refactor `validate()` enum checks to call `schema::find`. 23 existing tests in `tests/cpp/test_config.cpp` assert on exact error message substrings ("Invalid model size", "Invalid device", etc.) — every string must stay byte-identical. |
| 3 | Task 7 | `src/settings/handlers.h` | |
| 3 | Task 8 | `src/settings/handlers.cpp` + CMake | Pure JSON↔TOML bridge; ENOENT-tolerant load. |
| 3 | Task 9 | `tests/cpp/test_settings_handlers.cpp` | |
| 4 | Task 10 | `src/settings/sidecar.h` | Declarations only. |
| 4 | Task 11 | `src/settings/sidecar.cpp` + CMake | FNV-64, weak-canonical, `parse_sidecar`, `format_sidecar`. Flock/self-pipe IO stays in `cli_settings_ui.cpp` (Tasks 21-22). |
| 4 | Task 12 | `tests/cpp/test_sidecar.cpp` | |
| 5 | Task 13 | `cmake/EmbedAssets.cmake` | File-to-string-literal utility. |
| 5 | Task 14 | `src/settings/web/{index.html,style.css,app.js}` | Placeholder skeletons (real UI in Task 27). |
| 5 | Task 15 | `CMakeLists.txt` | Wire embedding + add `generated/` to autowhisper include path. |
| 6 | Task 16 | `src/cli/cli.{h,cpp}`, `src/cli/cli_settings_ui.cpp`, `CMakeLists.txt` | `config ui` subcommand placeholder. |
| 6 | Task 17 | `src/cli/cli_settings_ui.cpp` | Bind `httplib::Server` on `127.0.0.1:0`; `/api/schema` + `/api/defaults`. |
| 6 | Task 18 | `src/cli/cli_settings_ui.cpp` | `/api/config` GET (ENOENT → defaults) + PUT (validated save). |
| 6 | Task 19 | `tests/cpp/test_settings_http.cpp` | End-to-end via real `httplib::Server`. |
| 6 | Task 20 | `src/cli/cli_settings_ui.cpp` | Static asset routes `/`, `/style.css`, `/app.js`. |
| 6 | Task 21 | `src/cli/cli_settings_ui.cpp` | **Single-instance coordination.** Weak-canonicalize path, open sidecar with `O_CLOEXEC`, `flock(LOCK_EX \| LOCK_NB)`. Owner truncates BEFORE bind; writes AFTER bind. Loser polls 50ms × 60 retrying flock + reading sidecar. Takeover on mid-flight owner crash. |
| 6 | Task 22 | `src/cli/cli_settings_ui.cpp` | **Self-pipe shutdown.** `pipe2(O_CLOEXEC)` then `fcntl O_NONBLOCK` write end only. Handler does single `write()`. Watcher thread blocks on `read()` and calls `server.stop()`. **Do NOT unlink sidecar** — flock is inode-keyed, unlinking creates a race window. |
| 6 | Task 23 | `src/cli/cli_settings_ui.cpp` | `setsid()` + double-fork+exec `xdg-open`. |
| 7 | Task 24 | `src/tray/platform/tray_gtk.cpp` | Rewrite "Open Config" handler. `/proc/self/exe` (not PATH-search) + `g_spawn_async` with argv `{exe, "config", "ui", "--config", <path>}`. |
| 8 | Task 25 | delete `tools/autowhisper-settings`; remove install line | |
| 8 | Task 26 | `debian/control` | Remove `python3 (>= ...)` dep if present. Skip commit if no change. |
| 9 | Task 27 | `src/settings/web/*` | Real HTML/CSS/JS form generator. |
| 10 | Task 28 | end-to-end regression | Clean build, full ctest, three user-facing flows, tray handoff. |

## Execution protocol (how the plan is being run)

Subagent-driven development per `superpowers:subagent-driven-development`. Each task:

1. **Implementer subagent** — given verbatim task text + project context. Implements, tests, commits.
2. **Spec-compliance reviewer subagent** — confirms exact scope, enum parity, correct files touched, commit message exact. `PASS` or `FAIL`.
3. **Code-quality reviewer subagent** — judges style, safety, convention match. `APPROVED` or `NEEDS_CHANGES`.
4. If either reviewer finds issues, same implementer fixes (new commit, not amend) and reviewer re-reviews.
5. Mark task complete; move to next.

A fresh subagent is dispatched per task — they never share context with each other. The controller (you, or the agent running this workflow) provides the verbatim task text rather than asking the subagent to read the plan.

## Project constraints (apply to every task)

1. **Commit messages must not reference AI tooling.** A pre-commit hook in this repo rejects staged content (including commit messages) that pattern-matches common AI attribution trailers or vendor names. Use plain commit messages only. No `Co-Authored-By` trailers of any kind.
2. **Do not push.** Commit locally; the repo owner pushes at their own cadence.
3. **Do not touch files outside the scope of each task.** No drive-by cleanup, no formatting changes, no refactors.
4. **Leave `deps/whisper.cpp` alone.** Its `bindings/java/bin/` directory shows as untracked content in `git status` — that's normal Java build noise from the vendored submodule; ignore it.
5. **Branch is `next`.** Verify before starting. Don't switch branches.
6. **Submodule tags via `-b` don't work.** When the plan says `git submodule add -b v0.16.0 ...`, that fails because `-b` only accepts branches. Use instead:
   ```bash
   git submodule add <url> <path>
   (cd <path> && git fetch --tags --depth 1 origin <tag> && git checkout <tag>)
   ```
   Then add `shallow = true` to the new `.gitmodules` entry to match the repo convention from commit `9ffe87b`.
7. **External deps wire into `cmake/FetchDependencies.cmake`**, not the root `CMakeLists.txt`. Put new INTERFACE libraries next to `miniaudio` / `cpp_httplib` / `nlohmann_json`. Use `${CMAKE_SOURCE_DIR}`.

## Resumption checklist

When picking this back up:

1. Read this file plus `docs/plans/2026-04-19-settings-ui-cpp-rewrite-design.md` and `docs/plans/2026-04-19-settings-ui-cpp-rewrite-plan.md`.
2. `git status` — confirm on `next`, working tree clean modulo `deps/whisper.cpp` untracked noise.
3. `git log --oneline 7a077a9..HEAD` — verify the 6 Phase-1 commits are present.
4. `cmake --build build -j$(nproc) && cd build && ctest --output-on-failure` — expect 64/64 pass.
5. Start **Task 6** from the plan. Emphasize: every thrown error message string in `Config::validate()` (there are ~20) must remain byte-identical.

## Suggested next-task prompt template

If using an agent: dispatch a fresh subagent with the verbatim text of Task 6 from the plan, plus the "Project constraints" block above. Require a status report (DONE / DONE_WITH_CONCERNS / NEEDS_CONTEXT / BLOCKED) with the new commit SHA and confirmation that all 57 pre-existing tests + 7 schema tests still pass after the refactor. Dispatch spec reviewer then code-quality reviewer as described in the "Execution protocol" section above.

If working manually: skim `src/config/config.cpp:200-320`, port each enum check to call `schema::find(section, key)` + iterate `enum_values`, verify every `throw std::runtime_error("Invalid ...")` message string is unchanged, build, run ctest, commit with message `"Refactor Config::validate enum checks to use schema::find"` (no AI references).

## Out-of-scope items (deferred — do not add to this work)

- `autowhisper config set` schema-validation rewrite.
- Shell completion for `config set`.
- macOS/Windows tray integration (unsupported in v0.5.0).
- HTTP API authentication (localhost-only binding is the design).
- Live config reload from UI (daemon inotify already covers it).
- Always-on UI process (design is on-demand + single-instance).

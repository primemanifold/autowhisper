# Settings UI — C++ Rewrite + Shared Schema

Date: 2026-04-19

## Goal

Replace the standalone Python `tools/autowhisper-settings` tool with a C++ `autowhisper config ui` subcommand. Remove the Python 3.11+ runtime dependency, collapse to one language, and extract a shared `schema` module so `Config::validate()` and the settings UI stop duplicating allow-lists.

Also: fix the tray's broken "Open Config" handler (it currently runs `xdg-open` on the TOML file, which opens a text editor instead of the HTML UI) by routing it through the new subcommand with single-instance enforcement.

## Decisions

- **Entry point**: `autowhisper config ui` subcommand (not a separate binary).
- **Assets**: HTML/CSS/JS embedded at build time via a CMake step that converts files under `src/settings/web/` into C++ string literals (`src/settings/assets.h` generated).
- **Schema**: new `src/config/schema.{h,cpp}` module in `CORE_LIB_SOURCES`. Source of truth for section/key/type/enum/description/numeric bounds. Both `Config::validate()` and the settings UI read from it. **Defaults stay in `Config{}` member initializers** (heterogeneous types; a `std::variant` in `KeyDef` would add complexity for little gain). The UI fetches defaults via `GET /api/defaults`, which serializes `Config::default_config()`.
- **HTTP server**: `cpp-httplib` (header-only, MIT). New submodule at `deps/cpp-httplib`.
- **JSON**: `nlohmann/json` (header-only). New submodule at `deps/nlohmann-json`.
- **Browser launch**: `fork + exec("xdg-open", url)` on the URL (not the TOML file — that's the bug we're fixing). Matches the Python `webbrowser.open()` behavior.
- **Port**: ephemeral (bind port 0). Recorded in the sidecar file.
- **Single-instance coordination**: **per-config-path** sidecar at `$XDG_RUNTIME_DIR/autowhisper-settings-<hash>.info` (fallback `/tmp/autowhisper-settings-$UID-<hash>.info`), where `<hash>` is a hex-encoded FNV-64 of the **canonicalized-or-weak** config path (see below). This means `autowhisper config ui --config A.toml` and `autowhisper config ui --config B.toml` coexist as independent instances on different ports — and the UI also works when the target config doesn't exist yet (first-run / new-config flow, preserved from the Python version). Sidecar stores `<pid>\n<port>\n<canonical_path>\n`. Liveness is determined by **`flock(LOCK_EX | LOCK_NB)` held for the lifetime of the owning process** — not `kill(pid, 0)`, which is unreliable under PID reuse. The sidecar fd is opened with `O_CLOEXEC` (with `fcntl(F_SETFD, FD_CLOEXEC)` as a portable fallback), so child processes spawned during the UI's lifetime (browser, xdg-open) do not inherit the lock.
- **Tray**: "Open Config" menu item launches the **same binary the tray itself is running from** — resolved via `/proc/self/exe` (`readlink`), not `PATH` search — with argv `{/proc/self/exe, "config", "ui", "--config", <mgr->config_path_>}`, via `g_spawn_async()` **without** `G_SPAWN_SEARCH_PATH`. Two reasons:
    1. PATH search can pick a different install than the one running the daemon (e.g. `/usr/local/bin/autowhisper` vs `/usr/bin/autowhisper`) — the tray would then launch a UI built from a different version.
    2. The tray already does `/proc/self/exe`-relative lookups for its icon assets (`src/tray/platform/tray_gtk.cpp:41`), so the pattern is consistent.
  Passing `--config` is mandatory: the daemon's actual config path is carried in `config_path_` (`src/daemon/daemon.cpp:82`, `src/tray/platform/tray_gtk.cpp:143`), and without it the UI would default-search and possibly edit a different file. GLib handles reaping so no zombies accumulate across multiple launches.
- **Deletion**: `tools/autowhisper-settings` (Python), its `install(PROGRAMS …)` line in `CMakeLists.txt`, and any `python3 (>= 3.11)` edge in `debian/control`.

## Architecture

### Subcommand flow

```
autowhisper config ui [--config PATH] [--port N] [--no-browser]
  ├─ weakly-canonicalize config path (std::filesystem::weakly_canonical)
  ├─ FNV-64(canonical_path) → sidecar path
  ├─ open(sidecar, O_RDWR|O_CREAT|O_CLOEXEC, 0600); flock(LOCK_EX|LOCK_NB)
  │    ├─ lock acquired → we own this config's instance → proceed to bind
  │    └─ EWOULDBLOCK → loser path (poll 50 ms × up to 60 = 3 s):
  │           ├─ per iteration: re-attempt flock(LOCK_NB).
  │           │     If now LOCKED → owner died pre-publish; assume owner role.
  │           ├─ else read sidecar; if populated, open browser → exit 0
  │           └─ timeout: log "settings UI unresponsive", exit 1
  ├─ bind httplib::Server on 127.0.0.1:port (port=0 → kernel picks) FIRST
  ├─ ftruncate(fd, 0) + write `<pid>\n<port>\n<canonical_path>\n` AFTER bind
  ├─ `setsid()` so the process survives tray-parent exit
  ├─ fork+exec xdg-open on the URL (unless --no-browser)
  ├─ start shutdown watcher thread (blocks on `shutdown_requested.wait(false)`)
  ├─ install SIGINT/SIGTERM handlers — handler ONLY does
  │       `shutdown_requested.store(true, release); shutdown_requested.notify_all();`
  │   (async-signal-safe; matches `src/daemon/daemon.cpp:30-48`)
  ├─ watcher wakes → calls `server.stop()`, unlinks sidecar, returns
  └─ server.listen_after_bind()  (blocks; watcher.stop() causes this to return)
```

Key ordering rule: **the sidecar file stays empty until after the bind succeeds.** The loser never sees a bogus port because a placeholder port is never written — only a populated file is published. Losers that race in before the owner has bound poll-read until the file is non-empty (bounded at 3 seconds).

The flock handles crash recovery automatically: the kernel releases it when the owning process dies, so the next launch's `flock(LOCK_NB)` succeeds immediately.

### Schema module

`src/config/schema.{h,cpp}`:

```cpp
namespace autowhisper::schema {

enum class Type { String, Int, Float, Bool, StringArray, Enum };

struct KeyDef {
    std::string_view section;
    std::string_view key;
    Type type;
    std::vector<std::string_view> enum_values;   // empty unless Enum
    std::optional<double> min_numeric;
    std::optional<double> max_numeric;
    std::string_view description;
    // No default value here — defaults live in Config{} member initializers
    // and are exposed to the UI via GET /api/defaults.
};

const std::vector<KeyDef>& all();
const KeyDef* find(std::string_view section, std::string_view key);
nlohmann::json to_json();  // GET /api/schema payload

}  // namespace autowhisper::schema
```

Enum values today (extracted from `Config::validate` and `config.toml`):

- `model.size` — 16 whisper model names
- `model.device` — `cuda`, `cpu`, `auto`
- `model.compute_type` — `float16`, `float32`, `int8`, `int8_float16`, `int8_float32`, `int8_bfloat16`, `bfloat16`
- `hotkeys.mode` — `push_to_talk`, `toggle`
- `output.method` — `inject`, `clipboard`
- `output.ending_action` — `none`, `newline`, `return_key`
- `daemon.log_level` — `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off` (mirrors `Config::validate()` at `src/config/config.cpp:269-271` exactly; does not include `warning` because validate() does not accept it)

`Config::validate()` is refactored to iterate `schema::all()`. Error messages stay byte-identical ("Invalid model size", etc.) — 23 existing test assertions on those strings must not break.

### HTTP surface

| Method | Path | Body | Response |
|---|---|---|---|
| GET | `/` | — | embedded `index.html` |
| GET | `/style.css` | — | embedded CSS |
| GET | `/app.js` | — | embedded JS |
| GET | `/api/schema` | — | `schema::to_json()` (sections, keys, types, enums, bounds, descriptions) |
| GET | `/api/defaults` | — | `Config::default_config()` serialized as JSON (for the UI's "reset to defaults" affordance) |
| GET | `/api/config` | — | current TOML → JSON |
| PUT | `/api/config` | JSON config | validates against schema; writes TOML; `204` on success, `400 {errors: [...]}` on validation failure |
| POST | `/api/quit` | — | graceful shutdown (optional; needed for "Done" button) |

All responses JSON (except static assets). All handlers synchronous — no long-lived requests.

### Asset embedding

Add `cmake/EmbedAssets.cmake`: given a source file and a variable name, emit a `.h` with `constexpr std::string_view <name> = R"…"`. Called once for each file under `src/settings/web/`, with outputs written to `${CMAKE_BINARY_DIR}/generated/settings/`. The subcommand `#include`s `settings/assets.h` which aggregates them.

Raw string literal delimiter `)AUTOWHISPER_EMBED_END(` (long enough to never appear in HTML).

### Single-instance protocol

Sidecar file path: `<runtime_dir>/autowhisper-settings-<hash>.info`, where:

- `<runtime_dir>` = `$XDG_RUNTIME_DIR` if set and writable, else `/tmp`.
- `<hash>` = FNV-64 of the **weakly-canonicalized** config path, hex-encoded (16 chars).

**Weakly-canonicalized** means `std::filesystem::weakly_canonical(path)` (C++17): resolves and canonicalizes whatever prefix of the path actually exists, appends the non-existing suffix verbatim, and normalizes `..`/`.`/symlinks along the way. This is required because the UI must launch against a config file that does not exist yet (first-run / new-config flow) — the existing Python tool intentionally supports this, and `Config::save()` (`src/config/config.cpp:357`) already `mkdir -p`s the parent on write.

Example: `~/new.toml` where `~/` exists but `new.toml` does not → `/home/isura/new.toml` (canonical parent + literal leaf).

So each config file has its own independent instance — launching with `--config A.toml` and `--config B.toml` in parallel yields two servers on two ports, not a mis-routed reuse.

Content (exactly three lines, written only after the server is bound):
```
<pid>
<port>
<canonical_config_path>
```

Launch sequence:

1. Weakly-canonicalize the config path. Compute sidecar path.
2. `open(sidecar, O_RDWR|O_CREAT|O_CLOEXEC, 0600)` — get an fd unconditionally. If the platform lacks `O_CLOEXEC` in `open()`, follow with `fcntl(fd, F_SETFD, FD_CLOEXEC)`.
3. `flock(fd, LOCK_EX | LOCK_NB)`:
   - **Success** → we own this config's instance. Go to step 5.
   - **EWOULDBLOCK** → someone else owns it. Go to step 4.
4. **Loser path**: poll every 50 ms for up to 3 seconds. Each iteration:
   - Re-attempt `flock(fd, LOCK_EX | LOCK_NB)`. If it now succeeds, the previous owner died before publishing → **promote to owner** (go to step 5 with a truncated sidecar).
   - Otherwise read sidecar. If non-empty and well-formed, sanity-check the stored path against our canonical path (defense against an FNV-64 collision; mismatch → log and exit 1), open browser at `http://127.0.0.1:<port>`, exit 0.
   - If still empty and flock still held, sleep 50 ms and retry.
   - On timeout after 3 s with lock held and no published port: log and exit 1.
5. **Owner path**: leave the sidecar file empty for now. Bind the HTTP server on port 0 — kernel assigns a port. Only after bind succeeds: `ftruncate(fd, 0)`, write `<getpid()>\n<port>\n<canonical_path>\n`, fsync. Keep `fd` open and locked for the process lifetime.
6. Shutdown (normal flow driven by the watcher thread, not the signal handler): `server.stop()`, `unlink(sidecar)`, `close(fd)` (implicitly releases the lock). The signal handler itself is limited to the async-signal-safe atomic store + notify described in the launch diagram.

**Why the bind-before-publish rule**: if the owner wrote a placeholder port first, a racing loser could read `port=0` and try to open `http://127.0.0.1:0`. Since the owner only publishes after a successful bind, and the loser polls for a non-empty file, the loser always reads a real port or times out.

**Why `O_CLOEXEC`**: when the UI process forks `xdg-open` (which itself may fork a browser and exit), any inherited fds keep the flock held even after the UI server dies. That would break crash recovery — the next launch's `flock(LOCK_NB)` would fail because the browser still holds the inherited lock. `O_CLOEXEC` ensures the fd is closed on exec.

**Crash recovery**: when the owning process dies — clean, SIGKILL, segfault, whatever — the kernel releases the flock (assuming no CLOEXEC-less inheritance, covered above). The next launch's `flock(LOCK_NB)` succeeds in step 3 and takes over. No stale-file check logic needed, no PID-reuse hazard.

**Why not `kill(pid, 0)`**: PID reuse. After a reboot or long uptime, the stored PID may belong to an unrelated process — the check reports "alive" and we'd open the browser at a dead port forever.

## File layout

| Path | Change |
|---|---|
| `src/cli/cli.cpp` | add `config ui` subcommand hook |
| `src/cli/cli_settings_ui.cpp` | new — subcommand implementation (HTTP server, sidecar, browser launch) |
| `src/cli/cli.h` | declare `cmd_config_ui` |
| `src/config/schema.h` | new |
| `src/config/schema.cpp` | new — key table |
| `src/config/config.cpp` | refactor `validate()` to loop through `schema::all()` |
| `src/settings/web/index.html` | new — ported from Python inline HTML |
| `src/settings/web/style.css` | new |
| `src/settings/web/app.js` | new — fetches `/api/schema` + `/api/config`, POSTs back |
| `src/settings/assets.h` | generated at build time (gitignored) |
| `cmake/EmbedAssets.cmake` | new — file-to-string-literal utility |
| `CMakeLists.txt` | add `schema.cpp`, `cli_settings_ui.cpp`; add cpp-httplib + nlohmann/json submodules; add asset-embed step; remove `install(PROGRAMS tools/autowhisper-settings …)` |
| `deps/cpp-httplib/` | new submodule |
| `deps/nlohmann-json/` | new submodule |
| `src/tray/platform/tray_gtk.cpp` | rewrite Open Config handler to spawn `<self_exe> config ui --config <config_path_>` via `g_spawn_async()` with absolute `argv[0]` resolved from `/proc/self/exe`; stdout/stderr to `/dev/null` |
| `tools/autowhisper-settings` | deleted |
| `debian/control` | drop any `python3 (>= 3.11)` dependency |
| `tests/cpp/test_schema.cpp` | new — `schema::find`, `to_json` round-trips, enum membership |
| `tests/cpp/test_settings_http.cpp` | new — start server on ephemeral port, hit endpoints with httplib::Client, assert GET /api/config and PUT /api/config round-trip |
| `tests/cpp/test_config.cpp` | unchanged (existing 23 tests still pass — pinned message strings guard the validate() refactor) |

## Test plan

Unit:

- `schema::all()` returns a non-empty vector; each entry has non-empty section + key.
- `schema::find("model", "size")` returns a `KeyDef` with `Type::Enum` and the expected enum list.
- `schema::to_json()` emits a JSON object with one key per section, each an array of key definitions.
- Existing `test_config.cpp` stays green — proves the `validate()` refactor preserved messages.

Integration (new test file):

- Start `httplib::Server` on ephemeral port in a test thread with a fixture TOML.
- `GET /api/config` → expected JSON.
- `GET /api/schema` → matches `schema::to_json()`.
- `PUT /api/config` with valid body → `204`, TOML on disk updated.
- `PUT /api/config` with invalid enum value → `400`, TOML unchanged.
- Assets: `GET /` returns `Content-Type: text/html` and a non-empty body starting with `<!DOCTYPE html>`.

Not tested (acknowledged):

- Browser launch (requires display + browser installed).
- Single-instance flock race (stress-test territory, not unit-testable).
- Tray → subcommand handoff (GTK integration).

## Out of scope

- `autowhisper config set` schema validation — easy follow-up once `schema` exists; not in this work.
- Shell completion scripts (separate effort).
- CLI TUI — previously discussed and explicitly dropped (YAGNI).
- Live config reload from UI without restart — daemon already has inotify-based reload; unaffected.
- Authentication on the localhost API — localhost-only binding is the only "auth". Not adding tokens.
- macOS/Windows settings UI — daemon is Linux-only in v0.5.0.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| `validate()` refactor changes error strings → breaks 23 existing tests | Keep error strings byte-identical. The refactor is loop-driven but messages come from a per-key format string. |
| cpp-httplib bloats binary | Header-only; LTO strips unused parts. Measured ~30-50 KB addition. |
| Submodule additions slow clones | Already using shallow submodules (`9ffe87b`). |
| Asset regeneration on every build | CMake dependency tracks only the web source files. No rebuild if HTML unchanged. |
| Sidecar file survives crash with stale port | Flock is released by kernel on any process exit; next launch takes over cleanly. |
| Two clicks race on lock acquisition | Losing side's `flock(LOCK_NB)` returns EWOULDBLOCK, then reads the winner's port and opens the browser. No retry loop needed. |
| Two instances launched with different `--config` paths | Sidecar hash includes the canonical path, so the two instances use different sidecars and different locks — they coexist on different ports. |
| Losing launch reads a placeholder `port=0` before owner binds | Owner leaves the sidecar empty until bind succeeds; loser poll-reads (50 ms × up to 60 iterations = 3 s) for a populated file. Never sees port 0. |
| Lock fd inherited into `xdg-open` → browser holds lock after UI dies | `O_CLOEXEC` on the sidecar `open()` ensures the fd is closed on any `exec`. Verified by: after daemon-spawned UI exits, the next `config ui` must acquire the flock immediately. |
| Tray launches UI against wrong config file | Tray passes `--config <mgr->config_path_>` explicitly. The daemon-supplied path is the source of truth, not the UI's default-search. |
| FNV-64 hash collision across two config paths | FNV-64 collision space is 2^64; on a single user's machine with a handful of config paths, probability is negligible. Sanity-checking the canonical path stored in the sidecar (step 4) would detect a collision anyway and could be extended to spawn a new instance if needed. |

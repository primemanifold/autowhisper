# Settings UI — C++ Rewrite + Shared Schema

Date: 2026-04-19

## Goal

Replace the standalone Python `tools/autowhisper-settings` tool with a C++ `autowhisper config ui` subcommand. Remove the Python 3.11+ runtime dependency, collapse to one language, and extract a shared `schema` module so `Config::validate()` and the settings UI stop duplicating allow-lists.

Also: fix the tray's broken "Open Config" handler (it currently runs `xdg-open` on the TOML file, which opens a text editor instead of the HTML UI) by routing it through the new subcommand with single-instance enforcement.

## Decisions

- **Entry point**: `autowhisper config ui` subcommand (not a separate binary).
- **Assets**: HTML/CSS/JS embedded at build time via a CMake step that converts files under `src/settings/web/` into C++ string literals (`src/settings/assets.h` generated).
- **Schema**: new `src/config/schema.{h,cpp}` module in `CORE_LIB_SOURCES`. Single source of truth for section/key/type/enum/default/description/numeric bounds. Both `Config::validate()` and the settings UI read from it.
- **HTTP server**: `cpp-httplib` (header-only, MIT). New submodule at `deps/cpp-httplib`.
- **JSON**: `nlohmann/json` (header-only). New submodule at `deps/nlohmann-json`.
- **Browser launch**: `fork + exec("xdg-open", url)` on the URL (not the TOML file — that's the bug we're fixing). Matches the Python `webbrowser.open()` behavior.
- **Port**: ephemeral (bind port 0). Recorded in the sidecar file.
- **Single-instance coordination**: **per-config-path** sidecar at `$XDG_RUNTIME_DIR/autowhisper-settings-<hash>.info` (fallback `/tmp/autowhisper-settings-$UID-<hash>.info`), where `<hash>` is a hex-encoded FNV-64 of the canonical config path. This means `autowhisper config ui --config A.toml` and `autowhisper config ui --config B.toml` coexist as independent instances on different ports. Sidecar stores `<pid>\n<port>\n<canonical_config_path>\n`. Liveness is determined by **`flock(LOCK_EX | LOCK_NB)` held for the lifetime of the owning process** — not `kill(pid, 0)`, which is unreliable under PID reuse. A running owner holds the exclusive lock; a dead/crashed owner releases it automatically and the next launch takes over.
- **Tray**: "Open Config" menu item launches the main binary with `config ui` via `g_spawn_async()` (GLib is already linked for GTK). GLib handles reaping so no zombies accumulate if the daemon stays up across multiple launches.
- **Deletion**: `tools/autowhisper-settings` (Python), its `install(PROGRAMS …)` line in `CMakeLists.txt`, and any `python3 (>= 3.11)` edge in `debian/control`.

## Architecture

### Subcommand flow

```
autowhisper config ui [--config PATH] [--port N] [--no-browser]
  ├─ canonicalize config path (realpath); compute FNV-64 hash → sidecar path
  ├─ open(sidecar, O_RDWR|O_CREAT, 0600); flock(LOCK_EX|LOCK_NB)
  │    ├─ lock acquired → we own this config's instance → proceed
  │    └─ lock held by someone else → read port from sidecar
  │                                 → open browser at existing URL → exit 0
  ├─ write `<pid>\n0\n<canonical_path>\n` (port filled in after bind)
  ├─ bind httplib::Server on 127.0.0.1:port (port=0 → kernel picks)
  ├─ ftruncate + rewrite sidecar with real port
  ├─ open browser at http://127.0.0.1:PORT (unless --no-browser)
  ├─ `setsid()` so the process survives tray-parent exit
  ├─ install SIGINT/SIGTERM handlers → unlink sidecar + graceful server.stop()
  └─ server.listen_after_bind()  (blocks, holding the flock)
```

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
| GET | `/api/schema` | — | `schema::to_json()` |
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
- `<hash>` = FNV-64 of the canonicalized (`realpath`) config path, hex-encoded (16 chars).

So each config file has its own independent instance — launching with `--config A.toml` and `--config B.toml` in parallel yields two servers on two ports, not a mis-routed reuse.

Content (exactly three lines):
```
<pid>
<port>
<canonical_config_path>
```

Launch sequence:

1. Canonicalize the config path. Compute sidecar path.
2. `open(sidecar, O_RDWR|O_CREAT, 0600)` — get an fd unconditionally.
3. `flock(fd, LOCK_EX | LOCK_NB)`:
   - **Success** → we own this config's instance. Go to step 5.
   - **EWOULDBLOCK** → someone else owns it (and is alive, because flock is held for the process lifetime). Go to step 4.
4. Read the sidecar (pid, port, path). Sanity-check the path matches (defense against stale content from an earlier run with the same hash, should not happen but cheap to verify). Open browser at `http://127.0.0.1:<port>`. Exit 0.
5. `ftruncate(fd, 0)`, write `<getpid()>\n0\n<canonical_path>\n`. Bind HTTP server on port 0. Rewrite sidecar with the real port. Keep `fd` open and locked for the process lifetime.
6. On shutdown (SIGINT/SIGTERM/clean return): `unlink(sidecar)`, `close(fd)` (implicitly releases the lock).

**Crash recovery**: when the owning process dies — clean, SIGKILL, segfault, whatever — the kernel releases the flock. The next launch's `flock(LOCK_NB)` succeeds in step 3 and takes over. No stale-file check logic needed, no PID-reuse hazard.

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
| `src/tray/platform/tray_gtk.cpp` | rewrite Open Config handler to spawn `autowhisper config ui` detached |
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
| FNV-64 hash collision across two config paths | FNV-64 collision space is 2^64; on a single user's machine with a handful of config paths, probability is negligible. Sanity-checking the canonical path stored in the sidecar (step 4) would detect a collision anyway and could be extended to spawn a new instance if needed. |

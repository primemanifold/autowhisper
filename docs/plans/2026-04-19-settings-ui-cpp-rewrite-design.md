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
- **Single-instance coordination**: sidecar at `$XDG_RUNTIME_DIR/autowhisper-settings.info` (fallback `/tmp/autowhisper-settings-$UID.info`) storing `<pid>\n<port>\n`. Atomic creation via `O_CREAT|O_EXCL`. On re-launch with a live pid, just open the browser at the existing port and exit.
- **Tray**: "Open Config" menu item launches the main binary with `config ui` via `g_spawn_async()` (GLib is already linked for GTK). GLib handles reaping so no zombies accumulate if the daemon stays up across multiple launches.
- **Deletion**: `tools/autowhisper-settings` (Python), its `install(PROGRAMS …)` line in `CMakeLists.txt`, and any `python3 (>= 3.11)` edge in `debian/control`.

## Architecture

### Subcommand flow

```
autowhisper config ui [--config PATH] [--port N] [--no-browser]
  ├─ read sidecar; if pid alive → open browser at existing port → exit 0
  ├─ otherwise: create sidecar with O_CREAT|O_EXCL containing getpid()\n0
  ├─ bind httplib::Server on 127.0.0.1:port (port=0 → kernel picks)
  ├─ rewrite sidecar with actual chosen port
  ├─ open browser at http://127.0.0.1:PORT (unless --no-browser)
  ├─ `setsid()` so the process survives tray-parent exit
  ├─ install SIGINT/SIGTERM handlers → remove sidecar + graceful server.stop()
  └─ server.listen_after_bind()  (blocks)
```

If the sidecar exists but pid is not alive (stale after a crash), overwrite it.

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
- `daemon.log_level` — `trace`, `debug`, `info`, `warn`, `warning`, `error`

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

Sidecar file path: `<runtime_dir>/autowhisper-settings.info`.

Content: exactly two lines:
```
<pid>
<port>
```

Launch sequence:

1. Try `open(path, O_RDONLY)`. If present, parse pid+port.
2. If pid is alive (`kill(pid, 0) == 0` or ESRCH false), open browser at port, exit 0.
3. Otherwise (no file, or pid dead), attempt atomic claim: `open(path, O_WRONLY|O_CREAT|O_EXCL, 0600)`.
4. If EEXIST (race: another instance beat us), go to step 1. Bounded to 3 retries — if we still can't make progress, log and exit 1.
5. Write `getpid()\n0\n` to the claimed file.
6. Start server, then `lseek(0)`, `ftruncate(0)`, rewrite with real port.
7. On shutdown (SIGINT/SIGTERM/normal): `unlink(path)`.

Stale-file recovery: the step-2 check handles crashed instances.

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
| Sidecar file survives crash with stale port | `kill(pid, 0)` check on step 2 recovers. |
| Two clicks race on `O_CREAT\|O_EXCL` | Losing side retries from step 1 and finds the winner's entry. |

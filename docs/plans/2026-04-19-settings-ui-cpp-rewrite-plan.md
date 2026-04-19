# Settings UI C++ Rewrite — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Python `tools/autowhisper-settings` tool with a C++ `autowhisper config ui` subcommand, extract a shared config schema used by both `Config::validate()` and the UI, and fix the tray's broken "Open Config" handler along the way.

**Architecture:** New `src/config/schema.{h,cpp}` in `CORE_LIB_SOURCES` centralizes section/key/type/enum/bounds/description metadata. New `src/cli/cli_settings_ui.cpp` in `APP_SOURCES` hosts an `httplib::Server` on `127.0.0.1:0`, serves embedded HTML/CSS/JS plus JSON endpoints, and coordinates single-instance via a per-config `flock`-ed sidecar in `$XDG_RUNTIME_DIR`. Tray launches the subcommand via `/proc/self/exe` + `g_spawn_async`.

**Tech Stack:** C++20, CMake, existing `tomlplusplus` and `spdlog`; new submodules `cpp-httplib` (header-only HTTP), `nlohmann-json` (header-only JSON). Catch2 for tests. All per design doc `docs/plans/2026-04-19-settings-ui-cpp-rewrite-design.md`.

---

## File Structure

**New files:**

| Path | Responsibility |
|---|---|
| `deps/cpp-httplib/` | submodule at v0.16.0+ |
| `deps/nlohmann-json/` | submodule at v3.11.3+ |
| `cmake/EmbedAssets.cmake` | convert `web/*.{html,css,js}` into C++ string-literal headers at build time |
| `src/config/schema.h` | schema type definitions + free function declarations |
| `src/config/schema.cpp` | the key table (const data); `find()`, `to_json()` impls |
| `src/settings/handlers.h` | pure-logic HTTP handler functions (core lib) |
| `src/settings/handlers.cpp` | JSON ↔ TOML, validation, ENOENT-tolerant load |
| `src/settings/sidecar.h` | single-instance coordination types + declarations |
| `src/settings/sidecar.cpp` | weak-canonicalize + FNV-64 hash, flock, loser-path, self-pipe |
| `src/settings/web/index.html` | UI HTML (form generated from `/api/schema`) |
| `src/settings/web/style.css` | minimal CSS |
| `src/settings/web/app.js` | fetch schema+config, render form, POST back |
| `src/cli/cli_settings_ui.cpp` | subcommand entry: wire sidecar + httplib routes + browser launch (APP_SOURCES) |
| `tests/cpp/test_schema.cpp` | schema::find, schema::to_json, enum membership |
| `tests/cpp/test_settings_handlers.cpp` | handler pure-function tests (ENOENT, valid, invalid) |
| `tests/cpp/test_settings_http.cpp` | end-to-end via real httplib::Server on ephemeral port |
| `tests/cpp/test_sidecar.cpp` | weak-canonical, FNV-64, sidecar path construction |

**Modified files:**

| Path | Change |
|---|---|
| `CMakeLists.txt` | add submodules, add schema/handlers/sidecar to `CORE_LIB_SOURCES`, add `cli_settings_ui.cpp` to `APP_SOURCES`, invoke EmbedAssets, remove Python install target |
| `src/cli/cli.h` | declare `cmd_config_ui` |
| `src/cli/cli.cpp` | register `config ui` subcommand in `setup_cli` |
| `src/config/config.cpp` | refactor `validate()` to iterate `schema::all()`; preserve all error message strings byte-for-byte |
| `src/tray/platform/tray_gtk.cpp` | rewrite Open Config handler to spawn `/proc/self/exe config ui --config <path>` |
| `debian/control` | drop any `python3 (>= 3.11)` dependency edge if present |

**Deleted files:**

- `tools/autowhisper-settings` (Python script, 540+ lines)

---

## Phase 1 — Submodules and Schema Module

### Task 1: Add cpp-httplib submodule

**Files:**
- Create: `deps/cpp-httplib/` (submodule)
- Modify: `.gitmodules`, `CMakeLists.txt`

- [ ] **Step 1: Add submodule at v0.16.0**

```bash
git submodule add --depth 1 -b v0.16.0 https://github.com/yhirose/cpp-httplib.git deps/cpp-httplib
```

- [ ] **Step 2: Verify header is present**

```bash
test -f deps/cpp-httplib/httplib.h && echo OK
```

Expected: `OK`

- [ ] **Step 3: Wire cpp-httplib in CMakeLists.txt**

After the `find_package(Threads REQUIRED)` line, add:

```cmake
# cpp-httplib (header-only HTTP server)
add_library(cpp_httplib INTERFACE)
target_include_directories(cpp_httplib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib
)
```

- [ ] **Step 4: Build to confirm no regressions**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds with same targets as before (cpp_httplib is header-only, nothing links it yet).

- [ ] **Step 5: Commit**

```bash
git add .gitmodules deps/cpp-httplib CMakeLists.txt
git commit -m "Add cpp-httplib submodule at v0.16.0 for settings UI HTTP server"
```

---

### Task 2: Add nlohmann-json submodule

**Files:**
- Create: `deps/nlohmann-json/` (submodule)
- Modify: `.gitmodules`, `CMakeLists.txt`

- [ ] **Step 1: Add submodule at v3.11.3**

```bash
git submodule add --depth 1 -b v3.11.3 https://github.com/nlohmann/json.git deps/nlohmann-json
```

- [ ] **Step 2: Verify header is present**

```bash
test -f deps/nlohmann-json/single_include/nlohmann/json.hpp && echo OK
```

Expected: `OK`

- [ ] **Step 3: Wire nlohmann-json in CMakeLists.txt**

After the `cpp_httplib` INTERFACE library block, add:

```cmake
# nlohmann-json (header-only JSON)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/nlohmann-json/single_include
)
```

- [ ] **Step 4: Build to confirm**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add .gitmodules deps/nlohmann-json CMakeLists.txt
git commit -m "Add nlohmann-json submodule at v3.11.3 for settings UI JSON"
```

---

### Task 3: Schema header

**Files:**
- Create: `src/config/schema.h`

- [ ] **Step 1: Write header**

```cpp
#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace autowhisper::schema {

enum class Type {
    String,
    Int,
    Float,
    Bool,
    StringArray,
    Enum,
};

struct KeyDef {
    std::string_view section;
    std::string_view key;
    Type type;
    // Non-empty only when type == Enum.
    std::vector<std::string_view> enum_values;
    // Inclusive bounds on numeric types. nullopt = unbounded on that side.
    std::optional<double> min_numeric;
    std::optional<double> max_numeric;
    std::string_view description;
    // Defaults are NOT stored here — they live in Config{} member initializers
    // and are exposed to the UI via GET /api/defaults. See design doc.
};

// The full schema in declaration order. Stable across runs.
const std::vector<KeyDef>& all();

// Returns nullptr if (section, key) is not in the schema.
const KeyDef* find(std::string_view section, std::string_view key);

// Serialize the schema for the UI.
// Shape: { "<section>": [ { "key": "...", "type": "enum", "enum_values": [...],
//                           "min_numeric": <num or null>, "max_numeric": <num or null>,
//                           "description": "..." }, ... ], ... }
nlohmann::json to_json();

}  // namespace autowhisper::schema
```

- [ ] **Step 2: Commit**

```bash
git add src/config/schema.h
git commit -m "Add src/config/schema.h: shared config-key metadata types"
```

---

### Task 4: Schema key table (data only)

**Files:**
- Create: `src/config/schema.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write schema.cpp with the full key table**

Enum contents mirror `Config::validate()` in `src/config/config.cpp:269-271` for `daemon.log_level`, and the existing allow-lists in `validate()` for all other enums. Do NOT add or remove any value from those allow-lists.

```cpp
#include "config/schema.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace autowhisper::schema {

namespace {

using V = std::vector<std::string_view>;

// The canonical key table. Order matches config.toml section order.
const std::vector<KeyDef>& table() {
    static const std::vector<KeyDef> t = {
        // [model]
        {"model", "size", Type::Enum,
            V{"tiny", "tiny.en", "base", "base.en", "small", "small.en",
              "medium", "medium.en", "large", "large-v1", "large-v2", "large-v3",
              "distil-large-v2", "distil-large-v3", "distil-medium.en", "distil-small.en"},
            std::nullopt, std::nullopt,
            "Whisper model variant. Smaller = faster, larger = more accurate."},
        {"model", "device", Type::Enum,
            V{"cuda", "cpu", "auto"},
            std::nullopt, std::nullopt,
            "Inference device."},
        {"model", "compute_type", Type::Enum,
            V{"float16", "float32", "int8", "int8_float16",
              "int8_float32", "int8_bfloat16", "bfloat16"},
            std::nullopt, std::nullopt,
            "Numeric precision for inference."},
        {"model", "beam_size", Type::Int, V{}, 1.0, std::nullopt,
            "Beam search width. 1 = greedy."},
        {"model", "language", Type::String, V{}, std::nullopt, std::nullopt,
            "ISO 639-1 language code (e.g. 'en')."},
        {"model", "num_threads", Type::Int, V{}, 1.0, std::nullopt,
            "CPU threads for inference."},

        // [audio]
        {"audio", "sample_rate", Type::Int, V{}, 1.0, std::nullopt,
            "Capture sample rate in Hz. Whisper expects 16000."},
        {"audio", "channels", Type::Int, V{}, 1.0, std::nullopt,
            "Capture channel count."},
        {"audio", "buffer_size", Type::Int, V{}, 1.0, std::nullopt,
            "Audio buffer size in frames."},
        {"audio", "device", Type::String, V{}, std::nullopt, std::nullopt,
            "Input device name. Empty = default."},
        {"audio", "output_device", Type::String, V{}, std::nullopt, std::nullopt,
            "Output device for feedback tones. Empty = default."},
        {"audio", "vad_enabled", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Enable voice activity detection."},
        {"audio", "vad_threshold", Type::Float, V{}, 0.0, 1.0,
            "VAD activation threshold, 0..1."},
        {"audio", "silence_duration", Type::Float, V{}, 0.0, std::nullopt,
            "Silence duration in seconds before stop (toggle mode)."},
        {"audio", "max_duration", Type::Float, V{}, 0.0, std::nullopt,
            "Maximum recording length in seconds."},
        {"audio", "mute_other_apps", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Mute other applications while recording."},

        // [hotkeys]
        {"hotkeys", "mode", Type::Enum,
            V{"push_to_talk", "toggle"},
            std::nullopt, std::nullopt,
            "Hotkey activation mode."},
        {"hotkeys", "trigger", Type::StringArray, V{}, std::nullopt, std::nullopt,
            "One or more trigger hotkeys, e.g. ['shift+super']."},
        {"hotkeys", "cancel", Type::StringArray, V{}, std::nullopt, std::nullopt,
            "Cancel hotkeys, e.g. ['esc']."},
        {"hotkeys", "escape_to_cancel", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Treat Escape as cancel."},

        // [output]
        {"output", "method", Type::Enum,
            V{"inject", "clipboard"},
            std::nullopt, std::nullopt,
            "How transcribed text reaches the cursor."},
        {"output", "auto_paste", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Automatically paste after clipboard write."},
        {"output", "paste_delay", Type::Float, V{}, 0.0, std::nullopt,
            "Delay before paste, in seconds."},
        {"output", "ending_action", Type::Enum,
            V{"none", "newline", "return_key"},
            std::nullopt, std::nullopt,
            "What to append after the transcribed text."},
        {"output", "lowercase", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Lowercase the output."},
        {"output", "also_copy_to_clipboard", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Copy to clipboard in addition to the primary method."},

        // [feedback]
        {"feedback", "enabled", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Play tones on record start/stop/error."},
        {"feedback", "frequency_start", Type::Int, V{}, 1.0, std::nullopt,
            "Start tone frequency in Hz."},
        {"feedback", "frequency_stop", Type::Int, V{}, 1.0, std::nullopt,
            "Stop tone frequency in Hz."},
        {"feedback", "frequency_error", Type::Int, V{}, 1.0, std::nullopt,
            "Error tone frequency in Hz."},
        {"feedback", "duration", Type::Float, V{}, 0.0, std::nullopt,
            "Tone duration in seconds."},
        {"feedback", "volume", Type::Float, V{}, 0.0, 1.0,
            "Tone volume, 0..1."},

        // [daemon]
        {"daemon", "log_level", Type::Enum,
            V{"trace", "debug", "info", "warn", "error", "critical", "off"},
            std::nullopt, std::nullopt,
            "spdlog level."},
        {"daemon", "log_file", Type::String, V{}, std::nullopt, std::nullopt,
            "Optional log file path. Empty or 'default' = none."},
        {"daemon", "pid_file", Type::String, V{}, std::nullopt, std::nullopt,
            "PID file path. Must be non-empty."},
        {"daemon", "work_dir", Type::String, V{}, std::nullopt, std::nullopt,
            "Working directory. Must be non-empty."},

        // [tray]
        {"tray", "enabled", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Show the system tray icon."},
    };
    return t;
}

}  // namespace

const std::vector<KeyDef>& all() {
    return table();
}

const KeyDef* find(std::string_view section, std::string_view key) {
    const auto& t = table();
    auto it = std::find_if(t.begin(), t.end(), [&](const KeyDef& d) {
        return d.section == section && d.key == key;
    });
    return (it == t.end()) ? nullptr : &*it;
}

nlohmann::json to_json() {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& d : table()) {
        nlohmann::json entry;
        entry["key"] = std::string(d.key);
        switch (d.type) {
            case Type::String:      entry["type"] = "string"; break;
            case Type::Int:         entry["type"] = "int"; break;
            case Type::Float:       entry["type"] = "float"; break;
            case Type::Bool:        entry["type"] = "bool"; break;
            case Type::StringArray: entry["type"] = "string_array"; break;
            case Type::Enum:        entry["type"] = "enum"; break;
        }
        if (d.type == Type::Enum) {
            auto arr = nlohmann::json::array();
            for (auto v : d.enum_values) arr.push_back(std::string(v));
            entry["enum_values"] = std::move(arr);
        }
        entry["min_numeric"] = d.min_numeric ? nlohmann::json(*d.min_numeric) : nlohmann::json(nullptr);
        entry["max_numeric"] = d.max_numeric ? nlohmann::json(*d.max_numeric) : nlohmann::json(nullptr);
        entry["description"] = std::string(d.description);

        const std::string section_s(d.section);
        if (!out.contains(section_s)) out[section_s] = nlohmann::json::array();
        out[section_s].push_back(std::move(entry));
    }
    return out;
}

}  // namespace autowhisper::schema
```

- [ ] **Step 2: Wire into CORE_LIB_SOURCES**

In `CMakeLists.txt`, add `src/config/schema.cpp` to the `CORE_LIB_SOURCES` list (alphabetical position, after `config.cpp`):

```cmake
set(CORE_LIB_SOURCES
    src/config/config.cpp
    src/config/schema.cpp
    src/hotkey/hotkey_common.cpp
    src/models/models.cpp
    src/tray/tray_common.cpp
    src/util/logging.cpp
    src/util/subprocess.cpp
)
```

Then link `autowhisper_core` against `nlohmann_json`:

```cmake
target_link_libraries(autowhisper_core PUBLIC
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    nlohmann_json
    Threads::Threads
)
```

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/config/schema.cpp CMakeLists.txt
git commit -m "Add schema key table (data + JSON serialization), wire into core lib"
```

---

### Task 5: Schema unit tests

**Files:**
- Create: `tests/cpp/test_schema.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "config/schema.h"

#include <nlohmann/json.hpp>

using namespace autowhisper;

TEST_CASE("schema::all() returns non-empty key table", "[schema]") {
    const auto& t = schema::all();
    REQUIRE(!t.empty());
    for (const auto& d : t) {
        CHECK(!d.section.empty());
        CHECK(!d.key.empty());
    }
}

TEST_CASE("schema::find locates known keys", "[schema]") {
    auto* d = schema::find("model", "size");
    REQUIRE(d != nullptr);
    CHECK(d->type == schema::Type::Enum);
    CHECK(!d->enum_values.empty());
}

TEST_CASE("schema::find returns nullptr for unknown keys", "[schema]") {
    CHECK(schema::find("nonexistent_section", "foo") == nullptr);
    CHECK(schema::find("model", "nonexistent_key") == nullptr);
}

TEST_CASE("schema enum allow-lists match Config::validate", "[schema]") {
    // model.device
    auto* device = schema::find("model", "device");
    REQUIRE(device != nullptr);
    REQUIRE(device->type == schema::Type::Enum);
    CHECK(std::find(device->enum_values.begin(), device->enum_values.end(), "cuda")
          != device->enum_values.end());
    CHECK(std::find(device->enum_values.begin(), device->enum_values.end(), "cpu")
          != device->enum_values.end());
    CHECK(std::find(device->enum_values.begin(), device->enum_values.end(), "auto")
          != device->enum_values.end());
    CHECK(device->enum_values.size() == 3);

    // daemon.log_level must match config.cpp:269-271 exactly:
    // {"trace", "debug", "info", "warn", "error", "critical", "off"}
    auto* ll = schema::find("daemon", "log_level");
    REQUIRE(ll != nullptr);
    REQUIRE(ll->type == schema::Type::Enum);
    CHECK(ll->enum_values.size() == 7);
    for (auto v : {"trace", "debug", "info", "warn", "error", "critical", "off"}) {
        CHECK(std::find(ll->enum_values.begin(), ll->enum_values.end(), v)
              != ll->enum_values.end());
    }
    // Specifically does NOT include "warning".
    CHECK(std::find(ll->enum_values.begin(), ll->enum_values.end(), "warning")
          == ll->enum_values.end());
}

TEST_CASE("schema::to_json has one entry per section", "[schema]") {
    auto j = schema::to_json();
    REQUIRE(j.is_object());
    for (auto section : {"model", "audio", "hotkeys", "output", "feedback", "daemon", "tray"}) {
        CHECK(j.contains(section));
        CHECK(j[section].is_array());
        CHECK(!j[section].empty());
    }
}

TEST_CASE("schema::to_json serializes enum_values for enum keys", "[schema]") {
    auto j = schema::to_json();
    const auto& model_arr = j["model"];
    auto size_it = std::find_if(model_arr.begin(), model_arr.end(),
        [](const nlohmann::json& e) { return e["key"] == "size"; });
    REQUIRE(size_it != model_arr.end());
    CHECK((*size_it)["type"] == "enum");
    CHECK((*size_it)["enum_values"].is_array());
    CHECK(!(*size_it)["enum_values"].empty());
}

TEST_CASE("schema::to_json serializes numeric bounds", "[schema]") {
    auto j = schema::to_json();
    const auto& audio_arr = j["audio"];
    auto it = std::find_if(audio_arr.begin(), audio_arr.end(),
        [](const nlohmann::json& e) { return e["key"] == "vad_threshold"; });
    REQUIRE(it != audio_arr.end());
    CHECK((*it)["type"] == "float");
    CHECK((*it)["min_numeric"] == 0.0);
    CHECK((*it)["max_numeric"] == 1.0);
}
```

- [ ] **Step 2: Wire into tests CMakeLists**

In `CMakeLists.txt`, add to `autowhisper_tests` sources:

```cmake
add_executable(autowhisper_tests
    tests/cpp/test_config.cpp
    tests/cpp/test_keycombo.cpp
    tests/cpp/test_logging.cpp
    tests/cpp/test_models.cpp
    tests/cpp/test_schema.cpp
    tests/cpp/test_subprocess.cpp
    tests/cpp/test_tray_display.cpp
)
```

- [ ] **Step 3: Build and run only the schema tests**

```bash
cmake --build build -j$(nproc) --target autowhisper_tests
cd build && ctest --output-on-failure -R schema
```

Expected: all schema test cases pass.

- [ ] **Step 4: Run the full suite**

```bash
ctest --output-on-failure
```

Expected: all tests pass (existing 57 + new schema tests).

- [ ] **Step 5: Commit**

```bash
git add tests/cpp/test_schema.cpp CMakeLists.txt
git commit -m "Add schema unit tests: find, to_json, enum parity with validate()"
```

---

## Phase 2 — validate() Refactor

### Task 6: Refactor Config::validate() to read allow-lists from schema

**CRITICAL:** Existing `tests/cpp/test_config.cpp` asserts on specific error message substrings (e.g. "Invalid model size", "Invalid device", "Invalid compute_type", "Invalid hotkey mode", "Invalid output method", "Invalid ending_action", "Invalid volume", "Invalid beam_size", "Invalid num_threads", "Invalid channels", "Invalid vad_threshold", "Invalid paste_delay", "Invalid feedback frequencies", "Invalid feedback duration", "Invalid hotkeys.trigger", "Invalid hotkeys.cancel", "Invalid daemon.log_level"). **Every one of those strings must remain byte-identical after the refactor.**

**Files:**
- Modify: `src/config/config.cpp` (only the `validate()` function)

- [ ] **Step 1: Read the current validate() implementation in full**

```bash
sed -n '200,320p' src/config/config.cpp
```

Note every distinct throw message. Write them down — the refactored version must emit exactly these strings.

- [ ] **Step 2: Replace the enum checks in validate() with schema-driven checks**

The enum checks currently look like:

```cpp
static const std::set<std::string> valid_sizes = { ... };
if (!valid_sizes.count(model.size)) {
    throw std::runtime_error("Invalid model size: " + model.size);
}
```

Replace each hardcoded set with a single helper that looks up the schema:

```cpp
#include "config/schema.h"

namespace {
void check_enum(std::string_view section, std::string_view key,
                const std::string& value, std::string_view message_prefix) {
    const auto* d = schema::find(section, key);
    if (!d || d->type != schema::Type::Enum) return;  // schema drift — skip
    for (auto allowed : d->enum_values) {
        if (value == allowed) return;
    }
    throw std::runtime_error(std::string(message_prefix) + ": " + value);
}
}  // namespace
```

Then replace each enum check in `validate()`:

```cpp
// Before:
static const std::set<std::string> valid_sizes = {...};
if (!valid_sizes.count(model.size)) {
    throw std::runtime_error("Invalid model size: " + model.size);
}

// After:
check_enum("model", "size", model.size, "Invalid model size");
```

Do this for all 7 enum keys (`model.size`, `model.device`, `model.compute_type`, `hotkeys.mode`, `output.method`, `output.ending_action`, `daemon.log_level`). **Keep the prefix strings ("Invalid model size", "Invalid device", etc.) exactly as they were.**

- [ ] **Step 3: Leave the non-enum checks unchanged**

Numeric bounds (vad_threshold, volume, feedback frequencies, paste_delay, etc.), array emptiness (hotkeys.trigger, hotkeys.cancel), string emptiness (pid_file, work_dir) — leave those branches as-is. Schema-driven numeric bounds can be a follow-up; out of scope for this work.

- [ ] **Step 4: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 5: Run the FULL existing test suite — all must stay green**

```bash
cd build && ctest --output-on-failure
```

Expected: all tests pass — particularly the 23 tests in `test_config.cpp` that assert on validate() error messages. If any fail, the refactor has broken a message. Fix before proceeding.

- [ ] **Step 6: Commit**

```bash
git add src/config/config.cpp
git commit -m "Refactor Config::validate enum checks to use schema::find

Allow-lists now live in schema.cpp instead of being duplicated in
validate(). Error message strings are preserved byte-for-byte so
existing test_config.cpp assertions stay green."
```

---

## Phase 3 — Handler Logic (Pure Core)

### Task 7: Handler header

**Files:**
- Create: `src/settings/handlers.h`

- [ ] **Step 1: Write header**

```cpp
#pragma once

#include "config/config.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace autowhisper::settings {

// Load the config file at `path` and return it as JSON.
// If the file does not exist (ENOENT), return Config::default_config() as JSON.
// Any other error (malformed TOML, permission denied, etc.) throws.
nlohmann::json get_config_json(const std::string& path);

// Serialize a Config to JSON with the same shape as get_config_json.
nlohmann::json config_to_json(const Config& c);

// Deserialize JSON into a Config. Does NOT validate; caller should run
// Config::validate() afterward.
Config json_to_config(const nlohmann::json& j);

// Validation result bundle. errors is empty iff the config is valid.
struct ValidationResult {
    bool ok() const { return errors.empty(); }
    std::vector<std::string> errors;
};

// Validate by running Config::validate() inside a try/catch and capturing
// the thrown message. Returns one error per invalid field on the first
// failure (Config::validate throws on first error — this is a single-error
// wrapper, consistent with existing behavior).
ValidationResult validate_json(const nlohmann::json& j);

// Write the given config (as JSON) to `path` atomically, creating parent
// directories as needed (delegates to Config::save). Throws on I/O error.
void save_config_json(const std::string& path, const nlohmann::json& j);

// Serialize Config::default_config() as JSON.
nlohmann::json defaults_json();

}  // namespace autowhisper::settings
```

- [ ] **Step 2: Commit**

```bash
git add src/settings/handlers.h
git commit -m "Add settings handler header: pure JSON/TOML bridge functions"
```

---

### Task 8: Handler implementation

**Files:**
- Create: `src/settings/handlers.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Implement handlers**

```cpp
#include "settings/handlers.h"
#include "config/config.h"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace autowhisper::settings {

nlohmann::json config_to_json(const Config& c) {
    nlohmann::json j;

    j["model"] = {
        {"size", c.model.size},
        {"device", c.model.device},
        {"compute_type", c.model.compute_type},
        {"beam_size", c.model.beam_size},
        {"language", c.model.language},
        {"num_threads", c.model.num_threads},
    };

    j["audio"] = {
        {"sample_rate", c.audio.sample_rate},
        {"channels", c.audio.channels},
        {"buffer_size", c.audio.buffer_size},
        {"device", c.audio.device.value_or("")},
        {"output_device", c.audio.output_device.value_or("")},
        {"vad_enabled", c.audio.vad_enabled},
        {"vad_threshold", c.audio.vad_threshold},
        {"silence_duration", c.audio.silence_duration},
        {"max_duration", c.audio.max_duration},
        {"mute_other_apps", c.audio.mute_other_apps},
    };

    j["hotkeys"] = {
        {"mode", c.hotkeys.mode},
        {"trigger", c.hotkeys.trigger},
        {"cancel", c.hotkeys.cancel},
        {"escape_to_cancel", c.hotkeys.escape_to_cancel},
    };

    j["output"] = {
        {"method", c.output.method},
        {"auto_paste", c.output.auto_paste},
        {"paste_delay", c.output.paste_delay},
        {"ending_action", c.output.ending_action},
        {"lowercase", c.output.lowercase},
        {"also_copy_to_clipboard", c.output.also_copy_to_clipboard},
    };

    j["feedback"] = {
        {"enabled", c.feedback.enabled},
        {"frequency_start", c.feedback.frequency_start},
        {"frequency_stop", c.feedback.frequency_stop},
        {"frequency_error", c.feedback.frequency_error},
        {"duration", c.feedback.duration},
        {"volume", c.feedback.volume},
    };

    j["daemon"] = {
        {"log_level", c.daemon.log_level},
        {"log_file", c.daemon.log_file.value_or("")},
        {"pid_file", c.daemon.pid_file},
        {"work_dir", c.daemon.work_dir},
    };

    j["tray"] = {
        {"enabled", c.tray.enabled},
    };

    return j;
}

Config json_to_config(const nlohmann::json& j) {
    Config c = Config::default_config();

    auto get = [&](const char* section, const char* key, auto& dst) {
        if (j.contains(section) && j[section].contains(key)) {
            try { dst = j[section][key].get<std::decay_t<decltype(dst)>>(); }
            catch (...) { /* leave default */ }
        }
    };

    get("model", "size", c.model.size);
    get("model", "device", c.model.device);
    get("model", "compute_type", c.model.compute_type);
    get("model", "beam_size", c.model.beam_size);
    get("model", "language", c.model.language);
    get("model", "num_threads", c.model.num_threads);

    get("audio", "sample_rate", c.audio.sample_rate);
    get("audio", "channels", c.audio.channels);
    get("audio", "buffer_size", c.audio.buffer_size);
    get("audio", "vad_enabled", c.audio.vad_enabled);
    get("audio", "vad_threshold", c.audio.vad_threshold);
    get("audio", "silence_duration", c.audio.silence_duration);
    get("audio", "max_duration", c.audio.max_duration);
    get("audio", "mute_other_apps", c.audio.mute_other_apps);
    if (j.contains("audio") && j["audio"].contains("device")) {
        auto s = j["audio"]["device"].get<std::string>();
        c.audio.device = s.empty() ? std::nullopt : std::optional<std::string>(s);
    }
    if (j.contains("audio") && j["audio"].contains("output_device")) {
        auto s = j["audio"]["output_device"].get<std::string>();
        c.audio.output_device = s.empty() ? std::nullopt : std::optional<std::string>(s);
    }

    get("hotkeys", "mode", c.hotkeys.mode);
    get("hotkeys", "trigger", c.hotkeys.trigger);
    get("hotkeys", "cancel", c.hotkeys.cancel);
    get("hotkeys", "escape_to_cancel", c.hotkeys.escape_to_cancel);

    get("output", "method", c.output.method);
    get("output", "auto_paste", c.output.auto_paste);
    get("output", "paste_delay", c.output.paste_delay);
    get("output", "ending_action", c.output.ending_action);
    get("output", "lowercase", c.output.lowercase);
    get("output", "also_copy_to_clipboard", c.output.also_copy_to_clipboard);

    get("feedback", "enabled", c.feedback.enabled);
    get("feedback", "frequency_start", c.feedback.frequency_start);
    get("feedback", "frequency_stop", c.feedback.frequency_stop);
    get("feedback", "frequency_error", c.feedback.frequency_error);
    get("feedback", "duration", c.feedback.duration);
    get("feedback", "volume", c.feedback.volume);

    get("daemon", "log_level", c.daemon.log_level);
    if (j.contains("daemon") && j["daemon"].contains("log_file")) {
        auto s = j["daemon"]["log_file"].get<std::string>();
        c.daemon.log_file = (s.empty() || s == "default")
                              ? std::nullopt : std::optional<std::string>(s);
    }
    get("daemon", "pid_file", c.daemon.pid_file);
    get("daemon", "work_dir", c.daemon.work_dir);

    get("tray", "enabled", c.tray.enabled);

    return c;
}

nlohmann::json get_config_json(const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return config_to_json(Config::default_config());
    }
    Config c = Config::load(path);  // throws on other errors
    return config_to_json(c);
}

nlohmann::json defaults_json() {
    return config_to_json(Config::default_config());
}

ValidationResult validate_json(const nlohmann::json& j) {
    ValidationResult r;
    try {
        Config c = json_to_config(j);
        c.validate();
    } catch (const std::exception& e) {
        r.errors.emplace_back(e.what());
    }
    return r;
}

void save_config_json(const std::string& path, const nlohmann::json& j) {
    Config c = json_to_config(j);
    c.save(path);
}

}  // namespace autowhisper::settings
```

- [ ] **Step 2: Create src/settings/ directory and add to CORE_LIB_SOURCES**

```cmake
set(CORE_LIB_SOURCES
    src/config/config.cpp
    src/config/schema.cpp
    src/hotkey/hotkey_common.cpp
    src/models/models.cpp
    src/settings/handlers.cpp
    src/tray/tray_common.cpp
    src/util/logging.cpp
    src/util/subprocess.cpp
)
```

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/settings/handlers.h src/settings/handlers.cpp CMakeLists.txt
git commit -m "Add settings handlers: JSON <-> TOML bridge + ENOENT-tolerant load"
```

---

### Task 9: Handler unit tests

**Files:**
- Create: `tests/cpp/test_settings_handlers.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write tests**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "settings/handlers.h"
#include "config/config.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() /
               ("autowhisper_handlers_" + std::to_string(::getpid()) + "_" +
                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

}  // namespace

TEST_CASE("get_config_json returns defaults for missing file", "[handlers]") {
    TempDir tmp;
    auto path = (tmp.path / "nonexistent_subdir" / "new.toml").string();

    auto j = settings::get_config_json(path);
    REQUIRE(j.contains("model"));
    CHECK(j["model"]["size"] == Config::default_config().model.size);

    // File is NOT created by the GET path.
    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("get_config_json reads existing TOML", "[handlers]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    std::ofstream(path) << "[model]\nsize = \"tiny.en\"\n";

    auto j = settings::get_config_json(path);
    CHECK(j["model"]["size"] == "tiny.en");
    // Other sections merged from defaults.
    CHECK(j["audio"]["sample_rate"] == 16000);
}

TEST_CASE("defaults_json matches Config::default_config", "[handlers]") {
    auto j = settings::defaults_json();
    auto c = Config::default_config();
    CHECK(j["model"]["size"] == c.model.size);
    CHECK(j["model"]["device"] == c.model.device);
    CHECK(j["audio"]["sample_rate"] == c.audio.sample_rate);
}

TEST_CASE("validate_json accepts valid config", "[handlers]") {
    auto j = settings::defaults_json();
    auto r = settings::validate_json(j);
    CHECK(r.ok());
}

TEST_CASE("validate_json rejects invalid enum value", "[handlers]") {
    auto j = settings::defaults_json();
    j["model"]["size"] = "not-a-real-model";
    auto r = settings::validate_json(j);
    CHECK_FALSE(r.ok());
    REQUIRE(!r.errors.empty());
    CHECK(r.errors.front().find("Invalid model size") != std::string::npos);
}

TEST_CASE("save_config_json writes file and creates parents", "[handlers]") {
    TempDir tmp;
    auto path = (tmp.path / "sub1" / "sub2" / "new.toml").string();

    auto j = settings::defaults_json();
    j["model"]["size"] = "tiny.en";
    settings::save_config_json(path, j);

    REQUIRE(fs::exists(path));
    auto loaded = Config::load(path);
    CHECK(loaded.model.size == "tiny.en");
}
```

- [ ] **Step 2: Wire into tests**

In `CMakeLists.txt`, add `tests/cpp/test_settings_handlers.cpp` to `autowhisper_tests` sources.

- [ ] **Step 3: Build and run**

```bash
cmake --build build -j$(nproc) --target autowhisper_tests
cd build && ctest --output-on-failure -R handlers
```

Expected: all handler tests pass.

- [ ] **Step 4: Run the full suite**

```bash
ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/cpp/test_settings_handlers.cpp CMakeLists.txt
git commit -m "Add settings handlers unit tests: ENOENT, round-trip, validation"
```

---

## Phase 4 — Sidecar Coordination

### Task 10: Sidecar header

**Files:**
- Create: `src/settings/sidecar.h`

- [ ] **Step 1: Write header**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace autowhisper::settings {

// Compute FNV-64 of a byte sequence. Used for sidecar filename hashing.
// Not cryptographic; collision protection is the path-match check at
// loser-time (see design doc, section "Single-instance protocol").
uint64_t fnv64(std::string_view bytes);

// Hex-encode a 64-bit value as 16 lowercase hex chars.
std::string to_hex16(uint64_t v);

// Weakly-canonicalize a config path: resolves the existing prefix with
// realpath-like semantics and appends the non-existing suffix verbatim.
// Wrapper around std::filesystem::weakly_canonical that returns a string.
// Never throws; on error, returns the input path unchanged.
std::string weak_canonical(const std::string& path);

// Build the sidecar file path for a given canonical config path. Chooses
// $XDG_RUNTIME_DIR (no $UID suffix — already per-user) when set and
// writable, else /tmp with $UID suffix.
std::string sidecar_path_for(const std::string& canonical_config_path);

// What the owner writes into the sidecar after a successful bind.
struct SidecarContents {
    int pid = 0;
    int port = 0;
    std::string canonical_path;
};

// Parse sidecar file contents. Returns nullopt if file is empty or malformed.
std::optional<SidecarContents> parse_sidecar(std::string_view raw);

// Serialize sidecar contents. Always newline-terminated per line.
std::string format_sidecar(const SidecarContents& c);

}  // namespace autowhisper::settings
```

- [ ] **Step 2: Commit**

```bash
git add src/settings/sidecar.h
git commit -m "Add sidecar.h: path hashing, parse/format, weak canonicalization"
```

---

### Task 11: Sidecar helpers (weak-canonical, FNV-64, parse/format)

**Files:**
- Create: `src/settings/sidecar.cpp` (partial — the flock-and-self-pipe IO lives in the subcommand file later)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write implementation for the pure helpers**

```cpp
#include "settings/sidecar.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace autowhisper::settings {

uint64_t fnv64(std::string_view bytes) {
    // FNV-1a 64-bit.
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

std::string to_hex16(uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016lx", static_cast<unsigned long>(v));
    return std::string(buf);
}

std::string weak_canonical(const std::string& path) {
    std::error_code ec;
    auto p = fs::weakly_canonical(fs::path(path), ec);
    if (ec) return path;
    return p.string();
}

static bool is_dir_writable(const std::string& dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return false;
    return access(dir.c_str(), W_OK) == 0;
}

std::string sidecar_path_for(const std::string& canonical_config_path) {
    const std::string hash = to_hex16(fnv64(canonical_config_path));
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] && is_dir_writable(xdg)) {
        return std::string(xdg) + "/autowhisper-settings-" + hash + ".info";
    }
    // /tmp fallback: include $UID to avoid cross-user collisions.
    return "/tmp/autowhisper-settings-" + std::to_string(::getuid()) +
           "-" + hash + ".info";
}

std::optional<SidecarContents> parse_sidecar(std::string_view raw) {
    if (raw.empty()) return std::nullopt;
    std::string s(raw);
    std::istringstream is(s);
    SidecarContents c{};
    std::string pid_line, port_line, path_line;
    if (!std::getline(is, pid_line))  return std::nullopt;
    if (!std::getline(is, port_line)) return std::nullopt;
    if (!std::getline(is, path_line)) return std::nullopt;
    try {
        c.pid  = std::stoi(pid_line);
        c.port = std::stoi(port_line);
    } catch (...) {
        return std::nullopt;
    }
    if (c.pid <= 0 || c.port <= 0 || c.port > 65535) return std::nullopt;
    c.canonical_path = path_line;
    return c;
}

std::string format_sidecar(const SidecarContents& c) {
    std::ostringstream os;
    os << c.pid << "\n" << c.port << "\n" << c.canonical_path << "\n";
    return os.str();
}

}  // namespace autowhisper::settings
```

- [ ] **Step 2: Add to CORE_LIB_SOURCES**

```cmake
set(CORE_LIB_SOURCES
    src/config/config.cpp
    src/config/schema.cpp
    src/hotkey/hotkey_common.cpp
    src/models/models.cpp
    src/settings/handlers.cpp
    src/settings/sidecar.cpp
    src/tray/tray_common.cpp
    src/util/logging.cpp
    src/util/subprocess.cpp
)
```

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/settings/sidecar.cpp CMakeLists.txt
git commit -m "Add sidecar helpers: FNV-64, weak canonical, path construction, parse/format"
```

---

### Task 12: Sidecar unit tests

**Files:**
- Create: `tests/cpp/test_sidecar.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write tests**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "settings/sidecar.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace autowhisper::settings;

TEST_CASE("fnv64 is deterministic and non-trivial", "[sidecar]") {
    CHECK(fnv64("abc") == fnv64("abc"));
    CHECK(fnv64("abc") != fnv64("abd"));
    CHECK(fnv64("") == 0xcbf29ce484222325ULL);  // FNV-1a offset basis
}

TEST_CASE("to_hex16 returns exactly 16 chars, lowercase", "[sidecar]") {
    CHECK(to_hex16(0) == "0000000000000000");
    CHECK(to_hex16(0xdeadbeefULL).size() == 16);
    auto s = to_hex16(0xfeedfaceULL);
    CHECK(s.size() == 16);
    for (char ch : s) CHECK((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'));
}

TEST_CASE("weak_canonical tolerates non-existent suffix", "[sidecar]") {
    auto tmp = fs::temp_directory_path();
    // tmp definitely exists; append a non-existent suffix.
    auto p = (tmp / "autowhisper_nonexistent_dir" / "new.toml").string();
    auto c = weak_canonical(p);
    // Suffix must be preserved.
    CHECK(c.find("autowhisper_nonexistent_dir") != std::string::npos);
    CHECK(c.find("new.toml") != std::string::npos);
    // Existing prefix is resolved; path is absolute.
    CHECK(c.front() == '/');
}

TEST_CASE("sidecar_path_for uses XDG_RUNTIME_DIR when set", "[sidecar]") {
    // Save and mock.
    const char* saved = std::getenv("XDG_RUNTIME_DIR");
    auto tmp = fs::temp_directory_path() /
               ("autowhisper_xdg_" + std::to_string(::getpid()));
    fs::create_directories(tmp);
    setenv("XDG_RUNTIME_DIR", tmp.c_str(), 1);

    auto p = sidecar_path_for("/home/isura/.config/autowhisper/config.toml");
    CHECK(p.find(tmp.string()) == 0);
    CHECK(p.find("autowhisper-settings-") != std::string::npos);
    CHECK(p.ends_with(".info"));

    fs::remove_all(tmp);
    if (saved) setenv("XDG_RUNTIME_DIR", saved, 1);
    else unsetenv("XDG_RUNTIME_DIR");
}

TEST_CASE("sidecar_path_for falls back to /tmp with UID when no XDG", "[sidecar]") {
    const char* saved = std::getenv("XDG_RUNTIME_DIR");
    unsetenv("XDG_RUNTIME_DIR");

    auto p = sidecar_path_for("/x.toml");
    CHECK(p.find("/tmp/autowhisper-settings-") == 0);
    CHECK(p.find("-" + std::to_string(::getuid()) + "-") != std::string::npos);

    if (saved) setenv("XDG_RUNTIME_DIR", saved, 1);
}

TEST_CASE("parse_sidecar round-trip with format_sidecar", "[sidecar]") {
    SidecarContents c{12345, 38543, "/home/u/.config/autowhisper/config.toml"};
    auto s = format_sidecar(c);
    auto parsed = parse_sidecar(s);
    REQUIRE(parsed.has_value());
    CHECK(parsed->pid == 12345);
    CHECK(parsed->port == 38543);
    CHECK(parsed->canonical_path == c.canonical_path);
}

TEST_CASE("parse_sidecar rejects empty, malformed, or out-of-range input", "[sidecar]") {
    CHECK_FALSE(parse_sidecar("").has_value());
    CHECK_FALSE(parse_sidecar("onlyoneline").has_value());
    CHECK_FALSE(parse_sidecar("notanumber\n38543\n/x\n").has_value());
    CHECK_FALSE(parse_sidecar("123\n99999\n/x\n").has_value());  // port > 65535
    CHECK_FALSE(parse_sidecar("0\n38543\n/x\n").has_value());    // pid <= 0
    CHECK_FALSE(parse_sidecar("123\n0\n/x\n").has_value());      // port <= 0
}
```

- [ ] **Step 2: Wire into tests CMakeLists**

Add `tests/cpp/test_sidecar.cpp` to `autowhisper_tests` in `CMakeLists.txt`.

- [ ] **Step 3: Build and run**

```bash
cmake --build build -j$(nproc) --target autowhisper_tests
cd build && ctest --output-on-failure -R sidecar
```

Expected: all sidecar helper tests pass.

- [ ] **Step 4: Run full suite**

```bash
ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/cpp/test_sidecar.cpp CMakeLists.txt
git commit -m "Add sidecar helper unit tests: hash, path, parse/format"
```

---

## Phase 5 — Asset Embedding

### Task 13: EmbedAssets CMake module

**Files:**
- Create: `cmake/EmbedAssets.cmake`

- [ ] **Step 1: Write the script**

```cmake
# Convert a file into a C++ constexpr std::string_view header.
#
# Usage:
#   autowhisper_embed_asset(
#     SOURCE src/settings/web/index.html
#     OUTPUT ${CMAKE_BINARY_DIR}/generated/settings/index_html.h
#     VAR_NAME kIndexHtml)
#
# Emits:
#   #pragma once
#   #include <string_view>
#   namespace autowhisper::settings::assets {
#   inline constexpr std::string_view kIndexHtml = R"AW_ASSET(...)AW_ASSET";
#   }

function(autowhisper_embed_asset)
    set(oneValueArgs SOURCE OUTPUT VAR_NAME)
    cmake_parse_arguments(A "" "${oneValueArgs}" "" ${ARGN})

    file(READ "${A_SOURCE}" content)

    set(header "#pragma once\n#include <string_view>\nnamespace autowhisper::settings::assets {\ninline constexpr std::string_view ${A_VAR_NAME} = R\"AW_ASSET(${content})AW_ASSET\";\n}  // namespace\n")

    file(WRITE "${A_OUTPUT}" "${header}")
endfunction()

function(autowhisper_embed_web_assets OUT_DIR)
    file(MAKE_DIRECTORY "${OUT_DIR}")
    autowhisper_embed_asset(
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/index.html
        OUTPUT ${OUT_DIR}/index_html.h
        VAR_NAME kIndexHtml)
    autowhisper_embed_asset(
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/style.css
        OUTPUT ${OUT_DIR}/style_css.h
        VAR_NAME kStyleCss)
    autowhisper_embed_asset(
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/app.js
        OUTPUT ${OUT_DIR}/app_js.h
        VAR_NAME kAppJs)

    # Aggregate header so consumers can #include "settings/assets.h".
    file(WRITE "${OUT_DIR}/assets.h"
         "#pragma once\n#include \"index_html.h\"\n#include \"style_css.h\"\n#include \"app_js.h\"\n")
endfunction()
```

Important: the delimiter `AW_ASSET` must not appear in any embedded file. If it does, the build will fail with a cryptic parse error — choose a different delimiter then.

- [ ] **Step 2: Commit**

```bash
git add cmake/EmbedAssets.cmake
git commit -m "Add cmake/EmbedAssets.cmake: file -> C++ string_view header"
```

---

### Task 14: Minimal web assets (skeleton)

Enough to unblock Phase 6 end-to-end wiring. The full UI is written in Task 23.

**Files:**
- Create: `src/settings/web/index.html`
- Create: `src/settings/web/style.css`
- Create: `src/settings/web/app.js`

- [ ] **Step 1: Write placeholder index.html**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>AutoWhisper Settings</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <main>
    <h1>AutoWhisper Settings</h1>
    <div id="app">Loading...</div>
  </main>
  <script src="/app.js"></script>
</body>
</html>
```

- [ ] **Step 2: Write placeholder style.css**

```css
:root { color-scheme: light dark; font-family: system-ui, sans-serif; }
body { max-width: 720px; margin: 2rem auto; padding: 0 1rem; }
h1 { font-size: 1.5rem; }
```

- [ ] **Step 3: Write placeholder app.js**

```javascript
(async () => {
  const [schema, config] = await Promise.all([
    fetch("/api/schema").then(r => r.json()),
    fetch("/api/config").then(r => r.json()),
  ]);
  const el = document.getElementById("app");
  el.textContent = "Schema and config loaded. Full UI pending.";
  console.log("schema:", schema);
  console.log("config:", config);
})();
```

- [ ] **Step 4: Commit**

```bash
git add src/settings/web/
git commit -m "Add placeholder web assets (HTML/CSS/JS) for settings UI"
```

---

### Task 15: Wire asset embedding into CMake

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Include the EmbedAssets module and invoke it**

Near the top of `CMakeLists.txt` (after `include(cmake/PlatformSetup.cmake)`), add:

```cmake
include(cmake/EmbedAssets.cmake)
autowhisper_embed_web_assets(${CMAKE_BINARY_DIR}/generated/settings)
```

Note: this runs at CMake configure time (not build time). That's fine for static assets; re-running `cmake --build` is enough if you edit HTML/CSS/JS **and** re-run `cmake` (or touch `CMakeCache.txt`). If incremental-on-edit is needed later, promote this to `add_custom_command`. For now, manual `cmake .. -B build` after frontend edits is acceptable.

- [ ] **Step 2: Add generated dir to autowhisper's include path**

After `add_executable(autowhisper ${APP_SOURCES})`, add:

```cmake
target_include_directories(autowhisper PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)
```

- [ ] **Step 3: Re-run cmake and build**

```bash
cmake -B build
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: `${CMAKE_BINARY_DIR}/generated/settings/assets.h` exists and the main binary still builds.

```bash
ls build/generated/settings/
```

Expected: `app_js.h assets.h index_html.h style_css.h`

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "Wire asset embedding into CMake; include generated/ in autowhisper"
```

---

## Phase 6 — Settings UI Subcommand

This phase builds `cli_settings_ui.cpp` incrementally. It's the largest single file; each task adds a layer.

### Task 16: Declare cmd_config_ui and wire the subcommand

**Files:**
- Modify: `src/cli/cli.h`
- Modify: `src/cli/cli.cpp`
- Create: `src/cli/cli_settings_ui.cpp` (stub)

- [ ] **Step 1: Declare cmd_config_ui in cli.h**

Add to `src/cli/cli.h` inside the namespace, after `cmd_config_path`:

```cpp
int cmd_config_ui(const std::string& config_path, bool open_browser);
```

- [ ] **Step 2: Stub cli_settings_ui.cpp**

```cpp
#include "cli/cli.h"
#include "config/config.h"

#include <iostream>

namespace autowhisper {

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string path = config_path_opt.empty()
                               ? get_user_config_path()
                               : config_path_opt;
    std::cout << "Settings UI placeholder. Config path: " << path << "\n";
    return 0;
}

}  // namespace autowhisper
```

- [ ] **Step 3: Register config ui subcommand in setup_cli**

In `src/cli/cli.cpp`, inside `setup_cli`, after the existing `config_path = config_cmd->add_subcommand("path", ...)` block, add:

```cpp
static std::string ui_config;
static bool ui_no_browser = false;
auto* config_ui = config_cmd->add_subcommand("ui", "Open settings UI in browser");
config_ui->add_option("-c,--config", ui_config, "Config file path");
config_ui->add_flag("--no-browser", ui_no_browser, "Do not open a browser window");
config_ui->callback([]() {
    std::exit(cmd_config_ui(ui_config, !ui_no_browser));
});
```

- [ ] **Step 4: Add cli_settings_ui.cpp to APP_SOURCES**

In `CMakeLists.txt`:

```cmake
set(APP_SOURCES
    src/main.cpp
    src/daemon/daemon.cpp
    src/audio/audio.cpp
    src/inference/inference.cpp
    src/feedback/feedback.cpp
    src/cli/cli.cpp
    src/cli/cli_settings_ui.cpp
    src/doctor/doctor.cpp
    src/output/output_common.cpp
)
```

Link `autowhisper` against `cpp_httplib`:

```cmake
target_link_libraries(autowhisper PRIVATE
    autowhisper_core
    whisper
    CLI11::CLI11
    miniaudio
    cpp_httplib
)
```

- [ ] **Step 5: Build**

```bash
cmake -B build
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 6: Verify the subcommand exists**

```bash
./build/autowhisper config ui --help
```

Expected output includes `-c,--config` and `--no-browser`.

```bash
./build/autowhisper config ui --config /tmp/new.toml
```

Expected output: `Settings UI placeholder. Config path: /tmp/new.toml`

- [ ] **Step 7: Commit**

```bash
git add src/cli/cli.h src/cli/cli.cpp src/cli/cli_settings_ui.cpp CMakeLists.txt
git commit -m "Wire 'config ui' subcommand (placeholder implementation)"
```

---

### Task 17: HTTP server with /api/schema and /api/defaults

**Files:**
- Modify: `src/cli/cli_settings_ui.cpp`

- [ ] **Step 1: Replace the placeholder body**

```cpp
#include "cli/cli.h"
#include "config/config.h"
#include "config/schema.h"
#include "settings/handlers.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

namespace autowhisper {

namespace {

void register_api_routes(httplib::Server& srv) {
    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(settings::defaults_json().dump(), "application/json");
    });
}

}  // namespace

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string config_path = config_path_opt.empty()
                                      ? get_user_config_path()
                                      : config_path_opt;

    httplib::Server srv;
    register_api_routes(srv);

    // Bind on an ephemeral port.
    int port = srv.bind_to_any_port("127.0.0.1");
    if (port < 0) {
        std::cerr << "Failed to bind settings UI to localhost\n";
        return 1;
    }

    std::cout << "AutoWhisper Settings: http://127.0.0.1:" << port << "\n";
    std::cout << "Config file: " << config_path << "\n";
    std::cout << "Press Ctrl+C to close\n";

    srv.listen_after_bind();
    return 0;
}

}  // namespace autowhisper
```

- [ ] **Step 2: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-test manually**

In one terminal:

```bash
./build/autowhisper config ui --config /tmp/smoke.toml --no-browser
```

In another:

```bash
PORT=$(ss -tln | awk '/127.0.0.1/ {split($4, a, ":"); print a[2]}' | tail -1)
curl -s http://127.0.0.1:$PORT/api/schema | head -c 200
curl -s http://127.0.0.1:$PORT/api/defaults | head -c 200
```

Expected: both return JSON starting with `{`.

Ctrl+C the first terminal.

- [ ] **Step 4: Commit**

```bash
git add src/cli/cli_settings_ui.cpp
git commit -m "Settings UI: bind on ephemeral port, serve /api/schema and /api/defaults"
```

---

### Task 18: /api/config GET (ENOENT-tolerant) and PUT (validated save)

**Files:**
- Modify: `src/cli/cli_settings_ui.cpp`

- [ ] **Step 1: Extend register_api_routes**

Replace the `register_api_routes` helper with:

```cpp
void register_api_routes(httplib::Server& srv, const std::string& config_path) {
    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(settings::defaults_json().dump(), "application/json");
    });
    srv.Get("/api/config", [config_path](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(settings::get_config_json(config_path).dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(),
                            "application/json");
        }
    });
    srv.Put("/api/config", [config_path](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.body);
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"errors", {e.what()}}}.dump(),
                            "application/json");
            return;
        }
        auto v = settings::validate_json(body);
        if (!v.ok()) {
            res.status = 400;
            res.set_content(nlohmann::json{{"errors", v.errors}}.dump(),
                            "application/json");
            return;
        }
        try {
            settings::save_config_json(config_path, body);
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(),
                            "application/json");
            return;
        }
        res.status = 204;
    });
}
```

Update the call site in `cmd_config_ui`:

```cpp
register_api_routes(srv, config_path);
```

- [ ] **Step 2: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/cli/cli_settings_ui.cpp
git commit -m "Settings UI: /api/config GET (defaults on ENOENT) and PUT (validated save)"
```

---

### Task 19: Integration test for /api/* endpoints

**Files:**
- Create: `tests/cpp/test_settings_http.cpp`
- Modify: `CMakeLists.txt`

This test stands up a real `httplib::Server` in a thread, exercises endpoints with `httplib::Client`, and verifies behavior end-to-end. Because the route handlers live in `cli_settings_ui.cpp` (APP_SOURCES) — which the test binary does not link — we duplicate the *registration* logic inside the test (it's 30 lines) but still exercise the pure handlers from the core lib. This is the most pragmatic way to keep end-to-end coverage without promoting cli_settings_ui into core.

- [ ] **Step 1: Write the test**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "config/config.h"
#include "config/schema.h"
#include "settings/handlers.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

// Mirror of register_api_routes in cli_settings_ui.cpp so the test
// exercises the same endpoint shape without linking the app.
void register_routes(httplib::Server& srv, const std::string& path) {
    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& r) {
        r.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& r) {
        r.set_content(settings::defaults_json().dump(), "application/json");
    });
    srv.Get("/api/config", [path](const httplib::Request&, httplib::Response& r) {
        try {
            r.set_content(settings::get_config_json(path).dump(), "application/json");
        } catch (const std::exception& e) {
            r.status = 500;
            r.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    srv.Put("/api/config", [path](const httplib::Request& req, httplib::Response& r) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (const std::exception& e) {
            r.status = 400;
            r.set_content(nlohmann::json{{"errors", {e.what()}}}.dump(), "application/json");
            return;
        }
        auto v = settings::validate_json(body);
        if (!v.ok()) {
            r.status = 400;
            r.set_content(nlohmann::json{{"errors", v.errors}}.dump(), "application/json");
            return;
        }
        settings::save_config_json(path, body);
        r.status = 204;
    });
}

struct ServerFixture {
    httplib::Server srv;
    int port = 0;
    std::thread thread;
    ServerFixture(const std::string& path) {
        register_routes(srv, path);
        port = srv.bind_to_any_port("127.0.0.1");
        REQUIRE(port > 0);
        thread = std::thread([this]() { srv.listen_after_bind(); });
        // Wait for readiness.
        for (int i = 0; i < 50 && !srv.is_running(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE(srv.is_running());
    }
    ~ServerFixture() {
        srv.stop();
        if (thread.joinable()) thread.join();
    }
};

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("aw_http_" + std::to_string(::getpid()) + "_" +
            std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

}  // namespace

TEST_CASE("GET /api/schema returns schema JSON", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    std::ofstream(path) << "[model]\n";
    ServerFixture s(path);

    httplib::Client cli("127.0.0.1", s.port);
    auto res = cli.Get("/api/schema");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j.contains("model"));
    CHECK(j["model"].is_array());
}

TEST_CASE("GET /api/defaults matches Config::default_config", "[settings_http]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());
    std::ofstream((tmp.path / "c.toml")) << "";

    httplib::Client cli("127.0.0.1", s.port);
    auto res = cli.Get("/api/defaults");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j["model"]["size"] == Config::default_config().model.size);
}

TEST_CASE("GET /api/config returns defaults for nonexistent file", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "sub" / "new.toml").string();  // does not exist
    ServerFixture s(path);

    httplib::Client cli("127.0.0.1", s.port);
    auto res = cli.Get("/api/config");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j["model"]["size"] == Config::default_config().model.size);

    // GET must not create the file.
    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("PUT /api/config with valid body writes TOML and creates parents", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "sub1" / "sub2" / "new.toml").string();
    ServerFixture s(path);

    auto body = settings::defaults_json();
    body["model"]["size"] = "tiny.en";

    httplib::Client cli("127.0.0.1", s.port);
    auto res = cli.Put("/api/config", body.dump(), "application/json");
    REQUIRE(res);
    CHECK(res->status == 204);

    REQUIRE(fs::exists(path));
    auto loaded = Config::load(path);
    CHECK(loaded.model.size == "tiny.en");

    // Subsequent GET reflects the write.
    auto got = cli.Get("/api/config");
    REQUIRE(got);
    auto jget = nlohmann::json::parse(got->body);
    CHECK(jget["model"]["size"] == "tiny.en");
}

TEST_CASE("PUT /api/config with invalid enum returns 400 and does not write", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    ServerFixture s(path);

    auto body = settings::defaults_json();
    body["model"]["size"] = "not-a-real-model";

    httplib::Client cli("127.0.0.1", s.port);
    auto res = cli.Put("/api/config", body.dump(), "application/json");
    REQUIRE(res);
    CHECK(res->status == 400);

    auto j = nlohmann::json::parse(res->body);
    CHECK(j.contains("errors"));
    REQUIRE(j["errors"].is_array());
    REQUIRE(!j["errors"].empty());
    CHECK(std::string(j["errors"][0]).find("Invalid model size") != std::string::npos);

    // File must not be created on validation failure.
    CHECK_FALSE(fs::exists(path));
}
```

- [ ] **Step 2: Wire into tests, link cpp_httplib**

In `CMakeLists.txt`, add `tests/cpp/test_settings_http.cpp` to `autowhisper_tests`, and:

```cmake
target_link_libraries(autowhisper_tests PRIVATE
    autowhisper_core
    cpp_httplib
    Catch2::Catch2WithMain
)
```

- [ ] **Step 3: Build and run**

```bash
cmake --build build -j$(nproc) --target autowhisper_tests
cd build && ctest --output-on-failure -R settings_http
```

Expected: all HTTP integration tests pass.

- [ ] **Step 4: Run full suite**

```bash
ctest --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add tests/cpp/test_settings_http.cpp CMakeLists.txt
git commit -m "Add end-to-end HTTP tests for settings UI endpoints"
```

---

### Task 20: Static asset routes

**Files:**
- Modify: `src/cli/cli_settings_ui.cpp`

- [ ] **Step 1: Add static asset serving**

At the top of the file, add:

```cpp
#include "settings/assets.h"
```

After the `srv.Put("/api/config", ...)` registration, add three asset routes:

```cpp
srv.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content(std::string(settings::assets::kIndexHtml), "text/html");
});
srv.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
    res.set_content(std::string(settings::assets::kStyleCss), "text/css");
});
srv.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
    res.set_content(std::string(settings::assets::kAppJs), "application/javascript");
});
```

- [ ] **Step 2: Build and smoke-test**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Run the UI in one terminal:

```bash
./build/autowhisper config ui --config /tmp/smoke.toml --no-browser
```

In another terminal, find the port it printed, then:

```bash
curl -s http://127.0.0.1:PORT/ | head -c 200
curl -sI http://127.0.0.1:PORT/style.css | grep -i content-type
```

Expected: HTML body starting with `<!DOCTYPE html>`; `Content-Type: text/css`.

Ctrl+C the server.

- [ ] **Step 3: Commit**

```bash
git add src/cli/cli_settings_ui.cpp
git commit -m "Settings UI: serve embedded HTML/CSS/JS at /, /style.css, /app.js"
```

---

### Task 21: Single-instance coordination (flock + sidecar + loser path)

**Files:**
- Modify: `src/cli/cli_settings_ui.cpp`

This task implements the flock protocol described in the spec. Does not include self-pipe shutdown (Task 22) or browser launch (Task 23).

- [ ] **Step 1: Add includes**

At the top of `src/cli/cli_settings_ui.cpp`:

```cpp
#include "settings/sidecar.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <thread>
```

- [ ] **Step 2: Add sidecar-read helper**

Add this helper before `cmd_config_ui`:

```cpp
namespace {

std::string read_all(int fd) {
    std::string out;
    char buf[1024];
    if (lseek(fd, 0, SEEK_SET) < 0) return out;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, buf + n);
    }
    return out;
}

void write_full(int fd, const std::string& data) {
    if (ftruncate(fd, 0) < 0) return;
    if (lseek(fd, 0, SEEK_SET) < 0) return;
    size_t written = 0;
    while (written < data.size()) {
        ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        if (n <= 0) return;
        written += n;
    }
    fsync(fd);
}

// Open the sidecar with O_CLOEXEC and try to acquire the exclusive lock.
// Returns the fd (caller owns) and sets *got_lock. Returns -1 on open error.
int open_sidecar_locked(const std::string& path, bool* got_lock) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return -1;
    if (::flock(fd, LOCK_EX | LOCK_NB) == 0) { *got_lock = true; return fd; }
    *got_lock = false;
    return fd;
}

}  // namespace
```

- [ ] **Step 3: Rewrite cmd_config_ui to use flock**

Replace the `cmd_config_ui` body:

```cpp
int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string config_path = config_path_opt.empty()
                                      ? get_user_config_path()
                                      : config_path_opt;
    const std::string canonical = settings::weak_canonical(config_path);
    const std::string sidecar = settings::sidecar_path_for(canonical);

    bool got_lock = false;
    int fd = open_sidecar_locked(sidecar, &got_lock);
    if (fd < 0) {
        std::cerr << "Failed to open sidecar: " << sidecar << "\n";
        return 1;
    }

    // Loser path: poll for up to 3 seconds, also retrying flock in case
    // the owner dies pre-publish.
    if (!got_lock) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
                got_lock = true;
                break;  // promote to owner path below
            }
            auto parsed = settings::parse_sidecar(read_all(fd));
            if (parsed.has_value()) {
                if (parsed->canonical_path == canonical) {
                    std::cout << "Settings UI already running at http://127.0.0.1:"
                              << parsed->port << "\n";
                    ::close(fd);
                    return 0;
                }
                // Path mismatch: stale content from a previous owner that
                // crashed mid-takeover. Keep polling — current owner is
                // about to overwrite.
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!got_lock) {
            std::cerr << "Settings UI unresponsive (sidecar held but not published)\n";
            ::close(fd);
            return 1;
        }
    }

    // Owner path. Truncate immediately to clear stale content.
    if (::ftruncate(fd, 0) < 0) {
        std::cerr << "Failed to truncate sidecar: " << sidecar << "\n";
        ::close(fd);
        return 1;
    }

    httplib::Server srv;
    register_api_routes(srv, canonical);
    srv.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kIndexHtml), "text/html");
    });
    srv.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kStyleCss), "text/css");
    });
    srv.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kAppJs), "application/javascript");
    });

    int port = srv.bind_to_any_port("127.0.0.1");
    if (port < 0) {
        std::cerr << "Failed to bind settings UI to localhost\n";
        ::close(fd);
        return 1;
    }

    settings::SidecarContents contents{::getpid(), port, canonical};
    write_full(fd, settings::format_sidecar(contents));

    std::cout << "AutoWhisper Settings: http://127.0.0.1:" << port << "\n";
    std::cout << "Config file: " << canonical << "\n";
    std::cout << "Press Ctrl+C to close\n";

    srv.listen_after_bind();
    ::close(fd);  // releases the flock on normal exit
    return 0;
}
```

- [ ] **Step 4: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 5: Smoke-test single-instance behavior**

In terminal A:

```bash
./build/autowhisper config ui --config /tmp/single.toml --no-browser
```

Note the printed port (e.g. 38543).

In terminal B:

```bash
./build/autowhisper config ui --config /tmp/single.toml --no-browser
```

Expected: terminal B prints `Settings UI already running at http://127.0.0.1:38543` and exits 0.

Now try a different config in terminal C:

```bash
./build/autowhisper config ui --config /tmp/other.toml --no-browser
```

Expected: terminal C starts a new server on a different port.

Ctrl+C all.

- [ ] **Step 6: Commit**

```bash
git add src/cli/cli_settings_ui.cpp
git commit -m "Settings UI: flock-based single-instance + loser-path + takeover"
```

---

### Task 22: Self-pipe shutdown

**Files:**
- Modify: `src/cli/cli_settings_ui.cpp`

- [ ] **Step 1: Add includes**

```cpp
#include <csignal>
#include <cstring>
```

- [ ] **Step 2: Add self-pipe state and handler**

Inside the anonymous namespace:

```cpp
// Self-pipe for async-signal-safe shutdown.
// write end is non-blocking; read end is blocking.
static int g_signal_pipe[2] = {-1, -1};
static httplib::Server* g_server = nullptr;

void shutdown_signal_handler(int /*signo*/) {
    if (g_signal_pipe[1] >= 0) {
        char c = 'x';
        (void)!::write(g_signal_pipe[1], &c, 1);  // async-signal-safe
    }
}

bool install_signal_pipe_and_handlers() {
    if (::pipe(g_signal_pipe) < 0) return false;
    // O_CLOEXEC on both ends.
    ::fcntl(g_signal_pipe[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(g_signal_pipe[1], F_SETFD, FD_CLOEXEC);
    // Non-blocking on the write end only.
    int flags = ::fcntl(g_signal_pipe[1], F_GETFL, 0);
    ::fcntl(g_signal_pipe[1], F_SETFL, flags | O_NONBLOCK);

    struct sigaction sa{};
    sa.sa_handler = shutdown_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    return true;
}
```

- [ ] **Step 3: Start watcher thread in cmd_config_ui**

After `int port = srv.bind_to_any_port("127.0.0.1");` (successful bind) and before the sidecar write, insert:

```cpp
g_server = &srv;
if (!install_signal_pipe_and_handlers()) {
    std::cerr << "Failed to install signal-pipe shutdown\n";
    ::close(fd);
    return 1;
}

std::thread watcher([]() {
    char buf[1];
    // Blocking read — wakes when any SIGINT/SIGTERM fires.
    while (::read(g_signal_pipe[0], buf, 1) <= 0) {
        if (errno != EINTR) break;
    }
    if (g_server) g_server->stop();
});
```

After `srv.listen_after_bind();` add:

```cpp
watcher.join();
if (g_signal_pipe[0] >= 0) ::close(g_signal_pipe[0]);
if (g_signal_pipe[1] >= 0) ::close(g_signal_pipe[1]);
```

(Do NOT unlink the sidecar — see design doc for the reasoning: flock is inode-keyed, unlinking creates a race window.)

Also add `#include <cerrno>` at the top.

- [ ] **Step 4: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 5: Smoke-test graceful shutdown**

```bash
./build/autowhisper config ui --config /tmp/shutdown.toml --no-browser &
PID=$!
sleep 1
kill -TERM $PID
wait $PID
echo "exit=$?"
```

Expected: process exits promptly (within ~1 s), no hang.

- [ ] **Step 6: Commit**

```bash
git add src/cli/cli_settings_ui.cpp
git commit -m "Settings UI: self-pipe shutdown with watcher thread

Signal handler only does write(pipe, ...) which is POSIX-signal-safe.
Watcher blocks on read(), calls server.stop() from normal control flow.
Matches daemon.cpp pattern at src/daemon/daemon.cpp:30-48."
```

---

### Task 23: Browser launch + setsid

**Files:**
- Modify: `src/cli/cli_settings_ui.cpp`

- [ ] **Step 1: Add setsid() and xdg-open launcher**

At the top of the file:

```cpp
#include <sys/wait.h>
```

In the anonymous namespace, add:

```cpp
void launch_xdg_open(const std::string& url) {
    pid_t pid = ::fork();
    if (pid < 0) {
        spdlog::warn("fork failed; cannot launch browser");
        return;
    }
    if (pid == 0) {
        // Child: double-fork to detach from us, then exec xdg-open.
        if (::fork() == 0) {
            ::setsid();
            // Redirect stdio to /dev/null so the browser doesn't spam our tty.
            int devnull = ::open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                ::dup2(devnull, 0); ::dup2(devnull, 1); ::dup2(devnull, 2);
                if (devnull > 2) ::close(devnull);
            }
            ::execlp("xdg-open", "xdg-open", url.c_str(), (char*)nullptr);
            ::_exit(127);
        }
        ::_exit(0);  // immediate child exits; grandchild becomes init's
    }
    // Parent: reap the immediate child so no zombie lingers.
    int status = 0;
    ::waitpid(pid, &status, 0);
}
```

- [ ] **Step 2: Call setsid and launch browser from cmd_config_ui**

Directly after `settings::format_sidecar(contents)` (before the "AutoWhisper Settings:" stdout prints), add:

```cpp
::setsid();  // detach from tray's session so closing it doesn't kill us
```

After the "Press Ctrl+C to close" line and before `srv.listen_after_bind()`, add:

```cpp
if (open_browser) {
    launch_xdg_open("http://127.0.0.1:" + std::to_string(port));
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 4: Smoke-test**

With a browser installed:

```bash
./build/autowhisper config ui --config /tmp/browsertest.toml
```

Expected: browser opens to the localhost URL. The page loads and the console shows the schema+config JSON (per the placeholder JS).

Ctrl+C the server.

Repeat with `--no-browser`:

```bash
./build/autowhisper config ui --config /tmp/browsertest.toml --no-browser
```

Expected: no browser opens; the URL prints; Ctrl+C exits promptly.

- [ ] **Step 5: Commit**

```bash
git add src/cli/cli_settings_ui.cpp
git commit -m "Settings UI: setsid + xdg-open browser launch (double-fork, no zombies)"
```

---

## Phase 7 — Tray Integration

### Task 24: Rewrite Open Config handler in tray_gtk.cpp

**Files:**
- Modify: `src/tray/platform/tray_gtk.cpp`

- [ ] **Step 1: Replace the Open Config handler**

Find the existing Open Config signal connection at `src/tray/platform/tray_gtk.cpp:141-150`:

```cpp
g_signal_connect(config_item, "activate",
    G_CALLBACK(+[](GtkMenuItem*, gpointer data) {
        auto* mgr = static_cast<TrayManager*>(data);
        if (!mgr->config_path_.empty()) {
            std::string cmd = "xdg-open " + mgr->config_path_;
            if (system(cmd.c_str()) != 0) {
                spdlog::warn("Failed to open config with xdg-open");
            }
        }
    }), this);
```

Replace with:

```cpp
g_signal_connect(config_item, "activate",
    G_CALLBACK(+[](GtkMenuItem*, gpointer data) {
        auto* mgr = static_cast<TrayManager*>(data);
        if (mgr->config_path_.empty()) {
            spdlog::warn("No config path known; cannot launch settings UI");
            return;
        }
        // Resolve this binary's absolute path; never PATH-search.
        char exe_path[PATH_MAX];
        ssize_t n = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (n <= 0) {
            spdlog::warn("readlink(/proc/self/exe) failed; cannot launch settings UI");
            return;
        }
        exe_path[n] = '\0';

        gchar* argv[] = {
            exe_path,
            (gchar*)"config",
            (gchar*)"ui",
            (gchar*)"--config",
            (gchar*)mgr->config_path_.c_str(),
            nullptr,
        };
        GError* err = nullptr;
        gboolean ok = g_spawn_async(
            nullptr, argv, nullptr,
            (GSpawnFlags)(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
            nullptr, nullptr, nullptr, &err);
        if (!ok) {
            spdlog::warn("g_spawn_async failed: {}", err ? err->message : "unknown");
            if (err) g_error_free(err);
        }
    }), this);
```

- [ ] **Step 2: Add necessary includes**

At the top of `src/tray/platform/tray_gtk.cpp`, ensure:

```cpp
#include <climits>   // PATH_MAX
#include <unistd.h>  // readlink
```

(If already present, skip.)

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds.

- [ ] **Step 4: Smoke-test the tray handoff**

Start the daemon in foreground:

```bash
./build/autowhisper run --config /tmp/tray.toml &
```

(You may need to create /tmp/tray.toml first, or use your existing config.)

Click the tray "Open Config" menu item. Expected: the settings UI opens in the browser targeting `/tmp/tray.toml`. Clicking it a second time brings focus to the same tab (or opens a new tab pointing at the same port).

Kill the daemon.

- [ ] **Step 5: Commit**

```bash
git add src/tray/platform/tray_gtk.cpp
git commit -m "Tray: Open Config launches 'config ui --config <path>' via /proc/self/exe

Replaces the broken 'xdg-open <config.toml>' handler that opened the
TOML in a text editor instead of the web UI. Uses g_spawn_async without
G_SPAWN_SEARCH_PATH to guarantee the same binary as the running daemon."
```

---

## Phase 8 — Python Tool Removal and Debian

### Task 25: Delete the Python settings tool

**Files:**
- Delete: `tools/autowhisper-settings`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Remove the install line**

In `CMakeLists.txt`, delete:

```cmake
install(PROGRAMS tools/autowhisper-settings DESTINATION bin)
```

- [ ] **Step 2: Delete the Python script**

```bash
git rm tools/autowhisper-settings
```

- [ ] **Step 3: Build to confirm**

```bash
cmake -B build
cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: build succeeds; `tools/` directory no longer contains autowhisper-settings.

- [ ] **Step 4: Commit**

```bash
git add tools/ CMakeLists.txt
git commit -m "Remove Python autowhisper-settings tool (superseded by 'config ui')"
```

---

### Task 26: Update debian/control if python3 was listed

**Files:**
- Modify: `debian/control` (only if applicable)

- [ ] **Step 1: Check for Python dependency edge**

```bash
grep -n 'python3' debian/control || echo "no python3 dep"
```

- [ ] **Step 2: If any `python3 (>= ...)` line is in Depends/Recommends/Suggests, remove it**

Edit `debian/control` to drop the Python edge. If there is no Python edge, skip this task and do not make a commit.

- [ ] **Step 3: Commit (only if the file changed)**

```bash
git diff --cached debian/control | grep -q python && \
  git commit -m "debian/control: drop python3 dependency edge (settings UI is now C++)" || \
  echo "no change"
```

---

## Phase 9 — Frontend

### Task 27: Real HTML/CSS/JS settings UI

**Files:**
- Modify: `src/settings/web/index.html`
- Modify: `src/settings/web/style.css`
- Modify: `src/settings/web/app.js`

This is the user-facing UI polish. The placeholder from Task 14 already proves the wiring works. The real implementation replaces it with a functional form generator.

- [ ] **Step 1: Write the real index.html**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>AutoWhisper Settings</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <main>
    <header>
      <h1>AutoWhisper Settings</h1>
      <p id="config-path"></p>
    </header>
    <form id="settings-form"></form>
    <footer>
      <button id="save-btn" type="button">Save</button>
      <button id="reset-btn" type="button">Reset to defaults</button>
      <span id="status"></span>
    </footer>
  </main>
  <script src="/app.js"></script>
</body>
</html>
```

- [ ] **Step 2: Write the real style.css**

```css
:root { color-scheme: light dark; font-family: system-ui, -apple-system, sans-serif; }
* { box-sizing: border-box; }
body { max-width: 760px; margin: 2rem auto; padding: 0 1rem; line-height: 1.5; }
h1 { font-size: 1.5rem; margin: 0 0 .25rem 0; }
header p { margin: 0 0 1.5rem 0; color: #666; font-size: .9rem; font-family: monospace; }
section { margin: 1.5rem 0; border: 1px solid #ddd; border-radius: 6px; padding: 1rem; }
section h2 { margin: 0 0 .75rem 0; font-size: 1.1rem; text-transform: capitalize; }
.row { display: grid; grid-template-columns: 12rem 1fr; gap: .5rem 1rem; align-items: center; padding: .25rem 0; }
.row label { font-family: monospace; font-size: .85rem; }
.row .desc { grid-column: 2; color: #666; font-size: .8rem; }
.row input, .row select { font: inherit; padding: .25rem .5rem; }
footer { display: flex; gap: .5rem; align-items: center; margin-top: 1rem; position: sticky; bottom: 0; background: Canvas; padding: .5rem 0; }
button { font: inherit; padding: .5rem 1rem; cursor: pointer; }
#status { color: #666; font-size: .9rem; }
#status.ok { color: #080; }
#status.err { color: #c00; }
```

- [ ] **Step 3: Write the real app.js**

```javascript
(async () => {
  const [schema, config, defaults] = await Promise.all([
    fetch("/api/schema").then(r => r.json()),
    fetch("/api/config").then(r => r.json()),
    fetch("/api/defaults").then(r => r.json()),
  ]);

  const form = document.getElementById("settings-form");
  const pathEl = document.getElementById("config-path");
  const status = document.getElementById("status");

  const sections = Object.keys(schema);
  for (const section of sections) {
    const sec = document.createElement("section");
    sec.innerHTML = `<h2>${section}</h2>`;
    for (const keyDef of schema[section]) {
      sec.appendChild(renderRow(section, keyDef, config[section]?.[keyDef.key]));
    }
    form.appendChild(sec);
  }

  document.getElementById("save-btn").addEventListener("click", async () => {
    const body = collect();
    status.textContent = "Saving...";
    status.className = "";
    const res = await fetch("/api/config", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (res.status === 204) {
      status.textContent = "Saved.";
      status.className = "ok";
    } else {
      const j = await res.json();
      status.textContent = "Error: " + (j.errors?.join("; ") || j.error || res.statusText);
      status.className = "err";
    }
  });

  document.getElementById("reset-btn").addEventListener("click", () => {
    for (const section of sections) {
      for (const keyDef of schema[section]) {
        setInput(section, keyDef, defaults[section]?.[keyDef.key]);
      }
    }
    status.textContent = "Reset to defaults (not yet saved).";
    status.className = "";
  });

  function renderRow(section, keyDef, value) {
    const row = document.createElement("div");
    row.className = "row";
    const label = document.createElement("label");
    label.htmlFor = `${section}.${keyDef.key}`;
    label.textContent = `${section}.${keyDef.key}`;
    row.appendChild(label);
    row.appendChild(renderInput(section, keyDef, value));
    if (keyDef.description) {
      const d = document.createElement("div");
      d.className = "desc";
      d.textContent = keyDef.description;
      row.appendChild(d);
    }
    return row;
  }

  function renderInput(section, keyDef, value) {
    const id = `${section}.${keyDef.key}`;
    if (keyDef.type === "enum") {
      const sel = document.createElement("select");
      sel.id = id;
      for (const v of keyDef.enum_values) {
        const opt = document.createElement("option");
        opt.value = v; opt.textContent = v;
        if (v === value) opt.selected = true;
        sel.appendChild(opt);
      }
      return sel;
    }
    if (keyDef.type === "bool") {
      const cb = document.createElement("input");
      cb.type = "checkbox";
      cb.id = id;
      cb.checked = !!value;
      return cb;
    }
    if (keyDef.type === "int" || keyDef.type === "float") {
      const n = document.createElement("input");
      n.type = "number";
      n.id = id;
      if (keyDef.type === "float") n.step = "any";
      if (keyDef.min_numeric !== null) n.min = keyDef.min_numeric;
      if (keyDef.max_numeric !== null) n.max = keyDef.max_numeric;
      n.value = value ?? "";
      return n;
    }
    if (keyDef.type === "string_array") {
      const inp = document.createElement("input");
      inp.type = "text";
      inp.id = id;
      inp.placeholder = "comma-separated";
      inp.value = Array.isArray(value) ? value.join(", ") : "";
      return inp;
    }
    // string (default)
    const inp = document.createElement("input");
    inp.type = "text";
    inp.id = id;
    inp.value = value ?? "";
    return inp;
  }

  function setInput(section, keyDef, value) {
    const id = `${section}.${keyDef.key}`;
    const el = document.getElementById(id);
    if (!el) return;
    if (el.type === "checkbox") el.checked = !!value;
    else if (keyDef.type === "string_array")
      el.value = Array.isArray(value) ? value.join(", ") : "";
    else el.value = value ?? "";
  }

  function collect() {
    const out = {};
    for (const section of sections) {
      out[section] = {};
      for (const keyDef of schema[section]) {
        const el = document.getElementById(`${section}.${keyDef.key}`);
        if (!el) continue;
        if (el.type === "checkbox") out[section][keyDef.key] = el.checked;
        else if (el.type === "number")
          out[section][keyDef.key] = keyDef.type === "int" ? parseInt(el.value, 10) : parseFloat(el.value);
        else if (keyDef.type === "string_array")
          out[section][keyDef.key] = el.value.split(",").map(s => s.trim()).filter(Boolean);
        else out[section][keyDef.key] = el.value;
      }
    }
    return out;
  }
})();
```

- [ ] **Step 4: Re-run cmake so assets re-embed, build, smoke-test**

```bash
cmake -B build
cmake --build build -j$(nproc) --target autowhisper
./build/autowhisper config ui --config /tmp/ui.toml
```

Expected: the browser shows a populated form with inputs, a Save button, a Reset button. Changing a value and clicking Save writes to `/tmp/ui.toml`.

Invalid values (e.g. set `model.size` to something not in the dropdown — not possible with a select, but you could test via the API). The UI will already refuse out-of-range numerics client-side. Server-side validation is covered by PUT 400 behavior.

- [ ] **Step 5: Commit**

```bash
git add src/settings/web/
git commit -m "Settings UI: functional HTML/CSS/JS (form generator, save/reset)"
```

---

## Phase 10 — Final Verification

### Task 28: Full regression pass

- [ ] **Step 1: Clean build**

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Expected: clean build succeeds.

- [ ] **Step 2: Run the full test suite**

```bash
cd build && ctest --output-on-failure
```

Expected: 100% pass, count is the prior 57 + schema tests + handler tests + sidecar tests + http tests (total ~95-100).

- [ ] **Step 3: Manually verify the three user-facing flows**

Create `/tmp/end2end.toml`:

```bash
cp config.toml /tmp/end2end.toml
```

Run each and confirm behavior:

```bash
# 1. Direct CLI, existing file
./build/autowhisper config ui --config /tmp/end2end.toml
# Expect: browser opens, form populated.

# 2. Direct CLI, missing file
./build/autowhisper config ui --config /tmp/does_not_exist.toml
# Expect: browser opens, form shows defaults, Save creates the file.

# 3. Direct CLI, no --config
./build/autowhisper config ui
# Expect: browser opens; targets ~/.config/autowhisper/config.toml;
# if missing, Save creates it.
```

- [ ] **Step 4: Run the tray handoff end-to-end**

Start the daemon (foreground):

```bash
./build/autowhisper run --config /tmp/end2end.toml
```

Click the tray "Open Config" menu item.
Expected: browser opens at the settings UI targeting `/tmp/end2end.toml`.

Ctrl+C the daemon.

- [ ] **Step 5: No push yet**

Do not push. Leave all commits local until the user reviews.

---

## Out of scope — do not add to this plan

- `autowhisper config set` schema-validation rewrite (follow-up work).
- Shell completion for `config set` (separate effort).
- macOS/Windows tray integration (unsupported in v0.5.0).
- Authentication on the HTTP API (localhost-only binding is the only access control by design).
- Live config reload triggered from the UI (daemon's inotify already picks up changes on file save).
- Running the UI always-on (explicit user decision: on-demand + single-instance).

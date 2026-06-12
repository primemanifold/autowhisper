#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "config/config.h"

using Catch::Approx;

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace autowhisper;
using Catch::Matchers::ContainsSubstring;

namespace fs = std::filesystem;

namespace {

struct TempFile {
    fs::path path;

    explicit TempFile(const std::string& content) {
        path = fs::temp_directory_path() /
               ("autowhisper_test_" +
                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) +
                ".toml");
        std::ofstream ofs(path);
        ofs << content;
    }

    ~TempFile() {
        std::error_code ec;
        fs::remove(path, ec);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
};

struct TempConfigDir {
    fs::path path;

    TempConfigDir() {
        path = fs::temp_directory_path() /
               ("autowhisper_cfgdir_" +
                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        fs::create_directories(path);
    }

    ~TempConfigDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    TempConfigDir(const TempConfigDir&) = delete;
    TempConfigDir& operator=(const TempConfigDir&) = delete;
};

} // namespace

// ============================================================
// Default config
// ============================================================

TEST_CASE("Config::default_config returns valid defaults", "[config]") {
    auto cfg = Config::default_config();

    SECTION("model defaults") {
        CHECK(cfg.model.size == "distil-large-v3");
        CHECK(cfg.model.device == "cuda");
        CHECK(cfg.model.compute_type == "float16");
        CHECK(cfg.model.beam_size == 1);
        CHECK(cfg.model.language == "en");
        CHECK(cfg.model.num_threads == 4);
    }

    SECTION("audio defaults") {
        CHECK(cfg.audio.sample_rate == 16000);
        CHECK(cfg.audio.channels == 1);
        CHECK(cfg.audio.buffer_size == 1024);
        CHECK_FALSE(cfg.audio.device.has_value());
        CHECK_FALSE(cfg.audio.output_device.has_value());
        CHECK(cfg.audio.vad_enabled == true);
        CHECK(cfg.audio.vad_threshold == Approx(0.5f));
        CHECK(cfg.audio.silence_duration == Approx(0.3f));
        CHECK(cfg.audio.max_duration == Approx(600.0f));
        CHECK(cfg.audio.mute_other_apps == false);
    }

    SECTION("hotkey defaults") {
        CHECK(cfg.hotkeys.mode == "push_to_talk");
        REQUIRE(cfg.hotkeys.trigger.size() == 1);
        CHECK(cfg.hotkeys.trigger[0] == "shift+super");
        REQUIRE(cfg.hotkeys.cancel.size() == 1);
        CHECK(cfg.hotkeys.cancel[0] == "esc");
        CHECK(cfg.hotkeys.escape_to_cancel == true);
    }

    SECTION("output defaults") {
        CHECK(cfg.output.method == "inject");
        CHECK(cfg.output.auto_paste == true);
        CHECK(cfg.output.paste_delay == Approx(0.05f));
        CHECK(cfg.output.ending_action == "none");
        CHECK(cfg.output.lowercase == false);
        CHECK(cfg.output.also_copy_to_clipboard == true);
    }

    SECTION("feedback defaults") {
        CHECK(cfg.feedback.enabled == true);
        CHECK(cfg.feedback.frequency_start == 800);
        CHECK(cfg.feedback.frequency_stop == 400);
        CHECK(cfg.feedback.frequency_error == 600);
        CHECK(cfg.feedback.duration == Approx(0.1f));
        CHECK(cfg.feedback.volume == Approx(0.3f));
    }

    SECTION("daemon defaults") {
        CHECK(cfg.daemon.log_level == "info");
        CHECK_FALSE(cfg.daemon.log_file.has_value());
        CHECK(cfg.daemon.pid_file == "/tmp/autowhisper.pid");
        CHECK(cfg.daemon.work_dir == "/opt/autowhisper");
    }

    SECTION("tray defaults") {
        CHECK(cfg.tray.enabled == true);
    }

    SECTION("default config passes validation") {
        REQUIRE_NOTHROW(cfg.validate());
    }
}

// ============================================================
// Validation - valid values
// ============================================================

TEST_CASE("Config::validate accepts all valid model sizes", "[config][validate]") {
    auto cfg = Config::default_config();

    auto model_name = GENERATE(
        "tiny", "tiny.en", "base", "base.en", "small", "small.en",
        "medium", "medium.en", "large", "large-v1", "large-v2", "large-v3",
        "distil-large-v2", "distil-large-v3", "distil-medium.en", "distil-small.en"
    );

    cfg.model.size = model_name;
    REQUIRE_NOTHROW(cfg.validate());
}

TEST_CASE("Config::validate accepts valid devices", "[config][validate]") {
    auto cfg = Config::default_config();

    auto device = GENERATE("cuda", "cpu", "auto");
    cfg.model.device = device;
    REQUIRE_NOTHROW(cfg.validate());
}

TEST_CASE("Config::validate accepts all valid compute types", "[config][validate]") {
    auto cfg = Config::default_config();

    auto ct = GENERATE("float16", "float32", "int8", "int8_float16",
                        "int8_float32", "int8_bfloat16", "bfloat16");
    cfg.model.compute_type = ct;
    REQUIRE_NOTHROW(cfg.validate());
}

TEST_CASE("Config::validate accepts valid hotkey modes", "[config][validate]") {
    auto cfg = Config::default_config();

    auto mode = GENERATE("push_to_talk", "toggle");
    cfg.hotkeys.mode = mode;
    REQUIRE_NOTHROW(cfg.validate());
}

TEST_CASE("Config::validate accepts valid ending actions", "[config][validate]") {
    auto cfg = Config::default_config();

    auto action = GENERATE("none", "newline", "return_key");
    cfg.output.ending_action = action;
    REQUIRE_NOTHROW(cfg.validate());
}

// ============================================================
// Validation - invalid values
// ============================================================

TEST_CASE("Config::validate rejects invalid model size", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.model.size = "nonexistent";
    REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid model size"));
}

TEST_CASE("Config::validate rejects invalid device", "[config][validate]") {
    auto cfg = Config::default_config();

    auto device = GENERATE("gpu", "tpu", "");
    cfg.model.device = device;
    REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid device"));
}

TEST_CASE("Config::validate rejects invalid compute type", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.model.compute_type = "fp16";
    REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid compute_type"));
}

TEST_CASE("Config::validate rejects invalid hotkey mode", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.hotkeys.mode = "hold";
    REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid hotkey mode"));
}

TEST_CASE("Config::validate rejects invalid output method", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.output.method = "stdout";
    REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid output method"));
}

TEST_CASE("Config::validate rejects invalid ending_action", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.output.ending_action = "enter";
    REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid ending_action"));
}

TEST_CASE("Config::validate rejects out-of-range volume", "[config][validate]") {
    auto cfg = Config::default_config();

    SECTION("negative volume") {
        cfg.feedback.volume = -0.1f;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid volume"));
    }

    SECTION("volume too high") {
        cfg.feedback.volume = 1.1f;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid volume"));
    }

    SECTION("boundary low is valid") {
        cfg.feedback.volume = 0.0f;
        REQUIRE_NOTHROW(cfg.validate());
    }

    SECTION("boundary high is valid") {
        cfg.feedback.volume = 1.0f;
        REQUIRE_NOTHROW(cfg.validate());
    }
}

// ============================================================
// Load / Save / I/O
// ============================================================


TEST_CASE("Config::validate_all returns all validation errors", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.model.size = "nonexistent";
    cfg.model.beam_size = 0;
    cfg.audio.vad_threshold = 1.5f;
    cfg.feedback.volume = 2.0f;

    auto issues = cfg.validate_all();

    CHECK(issues.size() >= 4);
    CHECK(std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Error &&
               issue.path == "model.size" &&
               issue.code == "invalid_enum";
    }));
    CHECK(std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Error &&
               issue.path == "model.beam_size" &&
               issue.code == "out_of_range";
    }));
    CHECK(std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Error &&
               issue.path == "audio.vad_threshold" &&
               issue.code == "out_of_range";
    }));
    CHECK(std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Error &&
               issue.path == "feedback.volume" &&
               issue.code == "out_of_range";
    }));
}

TEST_CASE("ConfigValidator::validate returns validation issues without throwing", "[config][validate]") {
    auto cfg = Config::default_config();
    cfg.output.method = "stdout";

    auto issues = ConfigValidator::validate(cfg);

    REQUIRE(issues.size() == 1);
    CHECK(issues.front().path == "output.method");
    CHECK(issues.front().code == "invalid_enum");
    CHECK(issues.front().severity == ValidationSeverity::Error);
}

TEST_CASE("Config::validate_all returns empty for valid defaults", "[config][validate]") {
    auto cfg = Config::default_config();
    CHECK(cfg.validate_all().empty());
}

TEST_CASE("Config::save writes floats with shortest round-trip precision", "[config][io]") {
    TempConfigDir tmp;
    auto path = (tmp.path / "float.toml").string();

    auto cfg = Config::default_config();
    cfg.audio.silence_duration = 0.3f;
    cfg.audio.vad_threshold = 0.5f;
    cfg.output.paste_delay = 0.05f;
    cfg.save(path);

    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    CHECK(text.find("silence_duration = 0.3\n") != std::string::npos);
    CHECK(text.find("vad_threshold = 0.5\n") != std::string::npos);
    CHECK(text.find("paste_delay = 0.05\n") != std::string::npos);
    CHECK(text.find("0.30000001") == std::string::npos);

    // Values still round-trip exactly through load.
    auto loaded = Config::load(path);
    CHECK(loaded.audio.silence_duration == 0.3f);
    CHECK(loaded.audio.vad_threshold == 0.5f);
    CHECK(loaded.output.paste_delay == 0.05f);
}

TEST_CASE("Config::save round-trips awkward float values", "[config][io]") {
    TempConfigDir tmp;
    auto path = (tmp.path / "float2.toml").string();

    auto cfg = Config::default_config();
    cfg.audio.silence_duration = 1.0f / 3.0f;
    cfg.audio.vad_threshold = 0.123456789f;
    cfg.save(path);

    auto loaded = Config::load(path);
#if defined(__APPLE__)
    // macOS serializes floats at 6 significant digits (no float charconv on
    // libc++ with deployment target < 13.3); see Config::save.
    CHECK(std::abs(loaded.audio.silence_duration - 1.0f / 3.0f) < 1e-6f);
    CHECK(std::abs(loaded.audio.vad_threshold - 0.123456789f) < 1e-6f);
#else
    CHECK(loaded.audio.silence_duration == 1.0f / 3.0f);
    CHECK(loaded.audio.vad_threshold == 0.123456789f);
#endif
}

TEST_CASE("Config::validate_all warns on language/model mismatch", "[config][validate]") {
    auto cfg = Config::default_config();

    SECTION("auto language with English-only model warns") {
        cfg.model.size = "distil-small.en";
        cfg.model.language = "auto";
        auto issues = cfg.validate_all();
        REQUIRE(std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == ValidationSeverity::Warning &&
                   issue.path == "model.language" &&
                   issue.code == "language_model_mismatch";
        }));
    }

    SECTION("non-English language with English-only model warns") {
        cfg.model.size = "tiny.en";
        cfg.model.language = "de";
        auto issues = cfg.validate_all();
        REQUIRE(std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == ValidationSeverity::Warning &&
                   issue.code == "language_model_mismatch";
        }));
    }

    SECTION("auto language with multilingual model is clean") {
        cfg.model.size = "large-v3-turbo";
        cfg.model.language = "auto";
        CHECK(cfg.validate_all().empty());
    }

    SECTION("English language with English-only model is clean") {
        cfg.model.size = "distil-small.en";
        cfg.model.language = "en";
        CHECK(cfg.validate_all().empty());
    }
}

TEST_CASE("Config::validate rejects invalid numeric ranges", "[config][validate]") {
    auto cfg = Config::default_config();

    SECTION("invalid beam_size") {
        cfg.model.beam_size = 0;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid beam_size"));
    }

    SECTION("invalid num_threads") {
        cfg.model.num_threads = -1;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid num_threads"));
    }

    SECTION("invalid channels") {
        cfg.audio.channels = 0;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid channels"));
    }

    SECTION("invalid vad_threshold") {
        cfg.audio.vad_threshold = 1.5f;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid vad_threshold"));
    }

    SECTION("invalid paste_delay") {
        cfg.output.paste_delay = -0.1f;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid paste_delay"));
    }

    SECTION("invalid feedback frequency") {
        cfg.feedback.frequency_error = 0;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid feedback frequencies"));
    }

    SECTION("invalid feedback duration") {
        cfg.feedback.duration = 0.0f;
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid feedback duration"));
    }
}

TEST_CASE("Config::validate rejects invalid hotkey and daemon settings", "[config][validate]") {
    auto cfg = Config::default_config();

    SECTION("empty trigger list") {
        cfg.hotkeys.trigger.clear();
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid hotkeys.trigger"));
    }

    SECTION("empty cancel key entry") {
        cfg.hotkeys.cancel = {""};
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid hotkeys.cancel"));
    }

    SECTION("invalid daemon log level") {
        cfg.daemon.log_level = "verbose";
        REQUIRE_THROWS_WITH(cfg.validate(), ContainsSubstring("Invalid daemon.log_level"));
    }
}

TEST_CASE("Config::load_with_diagnostics warns on unknown TOML keys", "[config][io]") {
    TempFile tmp(R"(
[model]
size = "tiny.en"
extra_model_key = true

[unknown_section]
foo = "bar"
)");

    auto result = Config::load_with_diagnostics(tmp.path.string());

    CHECK(result.config.model.size == "tiny.en");
    CHECK(result.config.validate_all().empty());
    REQUIRE(result.issues.size() >= 2);
    CHECK(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Warning &&
               issue.path == "model.extra_model_key" &&
               issue.code == "unknown_key";
    }));
    CHECK(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Warning &&
               issue.path == "unknown_section" &&
               issue.code == "unknown_section";
    }));
}

TEST_CASE("Config::load_with_diagnostics returns validation errors without throwing", "[config][io]") {
    TempFile tmp(R"(
[model]
size = "nonexistent"
beam_size = 0
)");

    auto result = Config::load_with_diagnostics(tmp.path.string());

    CHECK(result.config.model.size == "nonexistent");
    CHECK(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Error &&
               issue.path == "model.size" &&
               issue.code == "invalid_enum";
    }));
    CHECK(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue) {
        return issue.severity == ValidationSeverity::Error &&
               issue.path == "model.beam_size" &&
               issue.code == "out_of_range";
    }));
}

TEST_CASE("Config::load_with_diagnostics allows schema_version and legacy append_newline", "[config][io]") {
    TempFile tmp(R"(
schema_version = 1

[output]
append_newline = true
)");

    auto result = Config::load_with_diagnostics(tmp.path.string());

    CHECK(result.config.output.ending_action == "newline");
    CHECK(std::none_of(result.issues.begin(), result.issues.end(), [](const auto& issue) {
        return issue.path == "schema_version" || issue.path == "output.append_newline";
    }));
}

TEST_CASE("Config::load throws on missing file", "[config][io]") {
    REQUIRE_THROWS_WITH(Config::load("/nonexistent/path/config.toml"),
                         ContainsSubstring("not found"));
}

TEST_CASE("Config::load throws on invalid TOML syntax", "[config][io]") {
    TempFile tmp("[invalid toml ===\n");
    REQUIRE_THROWS_WITH(Config::load(tmp.path.string()),
                         ContainsSubstring("TOML parse error"));
}

TEST_CASE("Config::load reads TOML file correctly", "[config][io]") {
    SECTION("partial TOML with only [model]") {
        TempFile tmp(R"(
[model]
size = "tiny.en"
device = "cpu"
)");
        auto cfg = Config::load(tmp.path.string());
        CHECK(cfg.model.size == "tiny.en");
        CHECK(cfg.model.device == "cpu");
        // Other sections should have defaults
        CHECK(cfg.audio.sample_rate == 16000);
        CHECK(cfg.hotkeys.mode == "push_to_talk");
    }

    SECTION("full TOML with all sections") {
        TempFile tmp(R"(
[model]
size = "base.en"
device = "auto"
compute_type = "float32"
beam_size = 5
language = "en"
num_threads = 8

[audio]
sample_rate = 16000
channels = 1
buffer_size = 2048
vad_enabled = false
vad_threshold = 0.3
silence_duration = 0.5
max_duration = 300.0
mute_other_apps = true

[hotkeys]
mode = "toggle"
trigger = ["ctrl+space"]
cancel = ["esc"]
escape_to_cancel = false

[output]
method = "clipboard"
auto_paste = false
paste_delay = 0.1
ending_action = "newline"
lowercase = true
also_copy_to_clipboard = false

[feedback]
enabled = false
frequency_start = 1000
frequency_stop = 500
frequency_error = 700
duration = 0.2
volume = 0.5

[daemon]
log_level = "debug"
pid_file = "/var/run/aw.pid"
work_dir = "/tmp/aw"

[tray]
enabled = false
)");
        auto cfg = Config::load(tmp.path.string());
        CHECK(cfg.model.size == "base.en");
        CHECK(cfg.model.device == "auto");
        CHECK(cfg.model.compute_type == "float32");
        CHECK(cfg.model.beam_size == 5);
        CHECK(cfg.model.num_threads == 8);
        CHECK(cfg.audio.buffer_size == 2048);
        CHECK(cfg.audio.vad_enabled == false);
        CHECK(cfg.audio.mute_other_apps == true);
        CHECK(cfg.audio.max_duration == Approx(300.0f));
        CHECK(cfg.hotkeys.mode == "toggle");
        REQUIRE(cfg.hotkeys.trigger.size() == 1);
        CHECK(cfg.hotkeys.trigger[0] == "ctrl+space");
        CHECK(cfg.hotkeys.escape_to_cancel == false);
        CHECK(cfg.output.method == "clipboard");
        CHECK(cfg.output.auto_paste == false);
        CHECK(cfg.output.ending_action == "newline");
        CHECK(cfg.output.lowercase == true);
        CHECK(cfg.output.also_copy_to_clipboard == false);
        CHECK(cfg.feedback.enabled == false);
        CHECK(cfg.feedback.frequency_start == 1000);
        CHECK(cfg.feedback.volume == Approx(0.5f));
        CHECK(cfg.daemon.log_level == "debug");
        CHECK(cfg.daemon.pid_file == "/var/run/aw.pid");
        CHECK(cfg.tray.enabled == false);
    }
}

TEST_CASE("Config::load handles backwards-compatible append_newline", "[config][io]") {
    SECTION("append_newline = true maps to newline") {
        TempFile tmp(R"(
[output]
append_newline = true
)");
        auto cfg = Config::load(tmp.path.string());
        CHECK(cfg.output.ending_action == "newline");
    }

    SECTION("append_newline = false maps to none") {
        TempFile tmp(R"(
[output]
append_newline = false
)");
        auto cfg = Config::load(tmp.path.string());
        CHECK(cfg.output.ending_action == "none");
    }
}

TEST_CASE("Config::load handles string and array trigger formats", "[config][io]") {
    SECTION("single string trigger") {
        TempFile tmp(R"(
[hotkeys]
trigger = "ctrl+space"
)");
        auto cfg = Config::load(tmp.path.string());
        REQUIRE(cfg.hotkeys.trigger.size() == 1);
        CHECK(cfg.hotkeys.trigger[0] == "ctrl+space");
    }

    SECTION("array of triggers") {
        TempFile tmp(R"(
[hotkeys]
trigger = ["ctrl+space", "shift+super"]
)");
        auto cfg = Config::load(tmp.path.string());
        REQUIRE(cfg.hotkeys.trigger.size() == 2);
        CHECK(cfg.hotkeys.trigger[0] == "ctrl+space");
        CHECK(cfg.hotkeys.trigger[1] == "shift+super");
    }
}


TEST_CASE("Config::load treats daemon.log_file default/empty as unset", "[config][io]") {
    SECTION("default sentinel") {
        TempFile tmp(R"(
[daemon]
log_file = "default"
)");
        auto cfg = Config::load(tmp.path.string());
        CHECK_FALSE(cfg.daemon.log_file.has_value());
    }

    SECTION("empty string") {
        TempFile tmp(R"(
[daemon]
log_file = ""
)");
        auto cfg = Config::load(tmp.path.string());
        CHECK_FALSE(cfg.daemon.log_file.has_value());
    }
}

TEST_CASE("Config round-trip save/load preserves values", "[config][io]") {
    Config original;
    original.model.size = "small.en";
    original.model.device = "cpu";
    original.model.compute_type = "float32";
    original.model.beam_size = 3;
    original.model.language = "en";
    original.model.num_threads = 2;
    original.audio.sample_rate = 16000;
    original.audio.channels = 1;
    original.audio.buffer_size = 512;
    original.audio.vad_enabled = false;
    original.audio.vad_threshold = 0.7f;
    original.audio.silence_duration = 0.1f;
    original.audio.max_duration = 120.0f;
    original.audio.mute_other_apps = true;
    original.hotkeys.mode = "toggle";
    original.hotkeys.trigger = {"ctrl+space", "shift+super"};
    original.hotkeys.cancel = {"esc", "ctrl+c"};
    original.hotkeys.escape_to_cancel = false;
    original.output.method = "clipboard";
    original.output.auto_paste = false;
    original.output.paste_delay = 0.2f;
    original.output.ending_action = "return_key";
    original.output.lowercase = true;
    original.output.also_copy_to_clipboard = false;
    original.feedback.enabled = false;
    original.feedback.frequency_start = 1000;
    original.feedback.frequency_stop = 500;
    original.feedback.frequency_error = 700;
    original.feedback.duration = 0.2f;
    original.feedback.volume = 0.8f;
    original.daemon.log_level = "debug";
    original.daemon.log_file = "/tmp/aw.log";
    original.daemon.pid_file = "/var/run/aw.pid";
    original.daemon.work_dir = "/tmp/aw";
    original.tray.enabled = false;

    auto tmppath = fs::temp_directory_path() / "autowhisper_roundtrip_test.toml";
    original.save(tmppath.string());
    auto loaded = Config::load(tmppath.string());
    fs::remove(tmppath);

    CHECK(loaded.model.size == original.model.size);
    CHECK(loaded.model.device == original.model.device);
    CHECK(loaded.model.compute_type == original.model.compute_type);
    CHECK(loaded.model.beam_size == original.model.beam_size);
    CHECK(loaded.model.num_threads == original.model.num_threads);
    CHECK(loaded.audio.buffer_size == original.audio.buffer_size);
    CHECK(loaded.audio.vad_enabled == original.audio.vad_enabled);
    CHECK(loaded.audio.vad_threshold == Approx(original.audio.vad_threshold));
    CHECK(loaded.audio.max_duration == Approx(original.audio.max_duration));
    CHECK(loaded.audio.mute_other_apps == original.audio.mute_other_apps);
    CHECK(loaded.hotkeys.mode == original.hotkeys.mode);
    CHECK(loaded.hotkeys.trigger == original.hotkeys.trigger);
    CHECK(loaded.hotkeys.cancel == original.hotkeys.cancel);
    CHECK(loaded.hotkeys.escape_to_cancel == original.hotkeys.escape_to_cancel);
    CHECK(loaded.output.method == original.output.method);
    CHECK(loaded.output.auto_paste == original.output.auto_paste);
    CHECK(loaded.output.paste_delay == Approx(original.output.paste_delay));
    CHECK(loaded.output.ending_action == original.output.ending_action);
    CHECK(loaded.output.lowercase == original.output.lowercase);
    CHECK(loaded.output.also_copy_to_clipboard == original.output.also_copy_to_clipboard);
    CHECK(loaded.feedback.enabled == original.feedback.enabled);
    CHECK(loaded.feedback.frequency_start == original.feedback.frequency_start);
    CHECK(loaded.feedback.frequency_stop == original.feedback.frequency_stop);
    CHECK(loaded.feedback.frequency_error == original.feedback.frequency_error);
    CHECK(loaded.feedback.duration == Approx(original.feedback.duration));
    CHECK(loaded.feedback.volume == Approx(original.feedback.volume));
    CHECK(loaded.daemon.log_level == original.daemon.log_level);
    CHECK(loaded.daemon.log_file == original.daemon.log_file);
    CHECK(loaded.daemon.pid_file == original.daemon.pid_file);
    CHECK(loaded.daemon.work_dir == original.daemon.work_dir);
    CHECK(loaded.tray.enabled == original.tray.enabled);
}

TEST_CASE("get_user_config_path uses HOME env", "[config]") {
    auto path = get_user_config_path();
    const char* home = std::getenv("HOME");

    if (home) {
        CHECK(path.find(home) == 0);
    }
    CHECK(path.find("/.config/autowhisper/config.toml") != std::string::npos);
}

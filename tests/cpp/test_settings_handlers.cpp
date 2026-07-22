#include <catch2/catch_test_macros.hpp>

#include "settings/handlers.h"
#include "config/config.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {
int test_process_id() {
#if defined(_WIN32)
    return _getpid();
#else
    return ::getpid();
#endif
}
}  // namespace


namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() /
               ("autowhisper_handlers_" + std::to_string(test_process_id()) + "_" +
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

    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("get_config_json reads existing TOML", "[handlers]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    std::ofstream(path) << "[model]\nsize = \"tiny.en\"\n";

    auto j = settings::get_config_json(path);
    CHECK(j["model"]["size"] == "tiny.en");
    CHECK(j["audio"]["sample_rate"] == 16000);
}

TEST_CASE("defaults_json matches Config::default_config", "[handlers]") {
    auto j = settings::defaults_json();
    auto c = Config::default_config();
    CHECK(j["model"]["size"] == c.model.size);
    CHECK(j["model"]["device"] == c.model.device);
    CHECK(j["audio"]["sample_rate"] == c.audio.sample_rate);
    CHECK(j["fabric"]["enabled"] == c.fabric.enabled);
    CHECK(j["fabric"]["executable"] == c.fabric.executable);
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

TEST_CASE("validate_json rejects type-mismatched value", "[handlers]") {
    auto j = settings::defaults_json();
    j["model"]["beam_size"] = "abc";
    auto r = settings::validate_json(j);
    CHECK_FALSE(r.ok());
    REQUIRE(!r.errors.empty());
    CHECK(r.errors.front().find("Invalid model.beam_size") != std::string::npos);
}

TEST_CASE("validate_json surfaces structured issues with path, code, severity", "[handlers]") {
    auto j = settings::defaults_json();
    j["model"]["size"] = "not-a-real-model";
    j["audio"]["channels"] = 99;
    auto r = settings::validate_json(j);

    CHECK_FALSE(r.ok());
    REQUIRE(r.issues.size() >= 2);

    auto find = [&](const std::string& path) {
        return std::find_if(r.issues.begin(), r.issues.end(),
            [&](const ValidationIssue& iss) { return iss.path == path; });
    };

    auto model_iss = find("model.size");
    REQUIRE(model_iss != r.issues.end());
    CHECK(model_iss->severity == ValidationSeverity::Error);
    CHECK(model_iss->code == "invalid_enum");
    CHECK(model_iss->message.find("Invalid model size") != std::string::npos);

    auto audio_iss = find("audio.channels");
    REQUIRE(audio_iss != r.issues.end());
    CHECK(audio_iss->severity == ValidationSeverity::Error);
    CHECK(audio_iss->code == "out_of_range");

    REQUIRE(r.errors.size() >= 2);
}

TEST_CASE("validate_json type-mismatch yields type_error issue with section.key path", "[handlers]") {
    auto j = settings::defaults_json();
    j["model"]["beam_size"] = "abc";
    auto r = settings::validate_json(j);

    CHECK_FALSE(r.ok());
    REQUIRE(r.issues.size() == 1);
    CHECK(r.issues.front().severity == ValidationSeverity::Error);
    CHECK(r.issues.front().path == "model.beam_size");
    CHECK(r.issues.front().code == "type_error");
    CHECK(r.issues.front().message.find("Invalid model.beam_size") != std::string::npos);
    REQUIRE(r.errors.size() == 1);
}

TEST_CASE("issue_to_json serializes severity, path, code, message", "[handlers]") {
    ValidationIssue iss;
    iss.severity = ValidationSeverity::Warning;
    iss.path = "audio.sample_rate";
    iss.code = "non_native_rate";
    iss.message = "Sample rate is not 16kHz";

    auto j = settings::issue_to_json(iss);
    CHECK(j["severity"] == "warning");
    CHECK(j["path"] == "audio.sample_rate");
    CHECK(j["code"] == "non_native_rate");
    CHECK(j["message"] == "Sample rate is not 16kHz");

    iss.severity = ValidationSeverity::Error;
    j = settings::issue_to_json(iss);
    CHECK(j["severity"] == "error");
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

TEST_CASE("models_json reports the catalog with on-disk availability", "[handlers]") {
    auto j = settings::models_json();
    REQUIRE(j.contains("models"));
    REQUIRE(j.contains("cache_dir"));
    REQUIRE(j["models"].is_array());
    REQUIRE(!j["models"].empty());

    for (const auto& m : j["models"]) {
        // Every entry must carry the fields the UI renders, and a single
        // boolean readiness flag — never a separate "ready" string.
        CHECK(m.contains("name"));
        CHECK(m.contains("description"));
        CHECK(m.contains("size"));
        CHECK(m.contains("english_only"));
        CHECK(m["downloaded"].is_boolean());
    }

    // The catalog names match the model module's source of truth.
    bool found_recommended = false;
    for (const auto& m : j["models"]) {
        if (m["name"] == "distil-small.en") found_recommended = true;
    }
    CHECK(found_recommended);
}

TEST_CASE("permissions_json reports applicability for the platform", "[handlers]") {
    auto j = settings::permissions_json();
    REQUIRE(j.contains("applicable"));
    REQUIRE(j.contains("permissions"));
    REQUIRE(j["permissions"].is_array());
#ifdef __APPLE__
    CHECK(j["applicable"] == true);
    CHECK(j["permissions"].size() == 3);
    for (const auto& p : j["permissions"]) {
        CHECK(p.contains("id"));
        CHECK(p.contains("state"));
        CHECK(p.contains("deep_link"));
    }
#else
    // No per-app permission model off macOS; the UI hides the pane.
    CHECK(j["applicable"] == false);
    CHECK(j["permissions"].empty());
#endif
}

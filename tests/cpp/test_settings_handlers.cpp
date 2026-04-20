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

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "config/schema.h"

#include <nlohmann/json.hpp>

#include <algorithm>

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

TEST_CASE("schema exposes a stable schema version", "[schema]") {
    CHECK(schema::version() == 1);
}

TEST_CASE("schema::find returns nullptr for unknown keys", "[schema]") {
    CHECK(schema::find("nonexistent_section", "foo") == nullptr);
    CHECK(schema::find("model", "nonexistent_key") == nullptr);
}

TEST_CASE("schema helpers distinguish known sections and keys", "[schema]") {
    CHECK(schema::is_known_section("model"));
    CHECK(schema::is_known_key("model", "size"));
    CHECK_FALSE(schema::is_known_section("unknown_section"));
    CHECK_FALSE(schema::is_known_key("model", "unknown_key"));
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

TEST_CASE("schema numeric bounds include Config::validate upper limits", "[schema]") {
    const auto* beam = schema::find("model", "beam_size");
    REQUIRE(beam != nullptr);
    REQUIRE(beam->max_numeric.has_value());
    CHECK(*beam->max_numeric == 10.0);

    const auto* threads = schema::find("model", "num_threads");
    REQUIRE(threads != nullptr);
    REQUIRE(threads->max_numeric.has_value());
    CHECK(*threads->max_numeric == 256.0);

    const auto* channels = schema::find("audio", "channels");
    REQUIRE(channels != nullptr);
    REQUIRE(channels->max_numeric.has_value());
    CHECK(*channels->max_numeric == 8.0);
}

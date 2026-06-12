#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "models/models.h"

#include <cstdlib>
#include <set>
#include <string>

using namespace autowhisper;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("MODEL_COUNT is correct", "[models]") {
    REQUIRE(MODEL_COUNT == 13);
}

TEST_CASE("MODELS array entries have valid fields", "[models]") {
    for (int i = 0; i < MODEL_COUNT; i++) {
        INFO("Model index: " << i << " name: " << MODELS[i].name);
        CHECK(std::string(MODELS[i].name).size() > 0);
        CHECK(std::string(MODELS[i].description).size() > 0);
        CHECK(std::string(MODELS[i].size).size() > 0);
        CHECK(std::string(MODELS[i].speed).size() > 0);

        std::string ggml = MODELS[i].ggml_file;
        CHECK(ggml.find("ggml-") == 0);
        CHECK(ggml.find(".bin") == ggml.size() - 4);

        std::string url = MODELS[i].url;
        CHECK(url.find("https://huggingface.co/") == 0);
        // The download URL must fetch exactly the file we cache and verify.
        CHECK(url.find(ggml) == url.size() - ggml.size());
    }
}

TEST_CASE("MODELS entries carry pinned SHA-256 checksums", "[models]") {
    for (int i = 0; i < MODEL_COUNT; i++) {
        INFO("Model index: " << i << " name: " << MODELS[i].name);
        std::string sha = MODELS[i].sha256;
        REQUIRE(sha.size() == 64);
        for (char c : sha) {
            bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            CHECK(hex);
        }
    }
}

TEST_CASE("MODELS array has expected model names", "[models]") {
    std::set<std::string> names;
    for (int i = 0; i < MODEL_COUNT; i++) {
        names.insert(MODELS[i].name);
    }

    CHECK(names.count("tiny.en") == 1);
    CHECK(names.count("tiny") == 1);
    CHECK(names.count("base.en") == 1);
    CHECK(names.count("base") == 1);
    CHECK(names.count("small.en") == 1);
    CHECK(names.count("small") == 1);
    CHECK(names.count("distil-small.en") == 1);
    CHECK(names.count("medium.en") == 1);
    CHECK(names.count("medium") == 1);
    CHECK(names.count("distil-medium.en") == 1);
    CHECK(names.count("distil-large-v3") == 1);
    CHECK(names.count("large-v3-turbo") == 1);
    CHECK(names.count("large-v3") == 1);
}

TEST_CASE("english_only flag matches whisper naming convention", "[models]") {
    for (int i = 0; i < MODEL_COUNT; i++) {
        std::string name = MODELS[i].name;
        INFO("Model: " << name);
        // English-only models end in ".en"; Distil-Whisper models are all
        // English-only regardless of suffix.
        bool en_suffix = name.size() > 3 && name.compare(name.size() - 3, 3, ".en") == 0;
        bool distil = name.rfind("distil-", 0) == 0;
        CHECK(MODELS[i].english_only == (en_suffix || distil));
    }
}

TEST_CASE("model_is_english_only uses catalog and falls back to convention", "[models]") {
    CHECK(model_is_english_only("tiny.en"));
    CHECK(model_is_english_only("distil-small.en"));
    CHECK(model_is_english_only("distil-large-v3"));
    CHECK_FALSE(model_is_english_only("large-v3-turbo"));
    CHECK_FALSE(model_is_english_only("large-v3"));
    // Not in catalog: naming convention decides.
    CHECK(model_is_english_only("custom-model.en"));
    CHECK(model_is_english_only("distil-large-v2"));
    CHECK_FALSE(model_is_english_only("large-v2"));
}

TEST_CASE("multilingual catalog covers every size tier", "[models]") {
    int multilingual = 0;
    for (int i = 0; i < MODEL_COUNT; i++) {
        if (!MODELS[i].english_only) multilingual++;
    }
    CHECK(multilingual >= 5);
}

TEST_CASE("get_model_path returns correct path for known models", "[models]") {
    for (int i = 0; i < MODEL_COUNT; i++) {
        std::string name = MODELS[i].name;
        INFO("Model: " << name);

        auto path = get_model_path(name);
        CHECK_FALSE(path.empty());
        CHECK(path.find("/.cache/whisper/") != std::string::npos);
        CHECK(path.find(MODELS[i].ggml_file) != std::string::npos);
    }
}

TEST_CASE("get_model_path returns empty for unknown model", "[models]") {
    CHECK(get_model_path("nonexistent").empty());
    CHECK(get_model_path("").empty());
    CHECK(get_model_path("whisper-large").empty());
}

TEST_CASE("is_model_downloaded returns false for unknown model", "[models]") {
    REQUIRE_FALSE(is_model_downloaded("nonexistent"));
    REQUIRE_FALSE(is_model_downloaded(""));
}

TEST_CASE("get_model_path uses HOME-based cache directory", "[models]") {
    const char* home = std::getenv("HOME");
    if (!home) {
        SKIP("HOME not set");
    }

    auto path = get_model_path("tiny.en");
    REQUIRE_FALSE(path.empty());
    CHECK(path.find(home) == 0);
    CHECK(path.find("/.cache/whisper/") != std::string::npos);
}

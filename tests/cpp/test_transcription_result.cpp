#include <catch2/catch_test_macros.hpp>

#include "runtime/transcription.h"

using namespace autowhisper;

TEST_CASE("TranscriptionResult v1 round trips without losing structured fields", "[runtime][protocol]") {
    TranscriptionResult result;
    result.request_id = "req-42";
    result.status = TranscriptionStatus::completed;
    result.text = "Ship the voice note workflow.";
    result.language = "en";
    result.duration_ms = 1840;
    result.processing_ms = 412;
    result.model = "small.en";
    result.segments.push_back({0, 1840, result.text});
    result.warnings.push_back("fixture warning");

    const nlohmann::json encoded = result;
    CHECK(encoded.at("schema") == "fabric.transcription");
    CHECK(encoded.at("version") == 1);
    CHECK(encoded.at("status") == "completed");
    CHECK(encoded.at("provider") == "autowhisper");

    const auto decoded = encoded.get<TranscriptionResult>();
    CHECK(decoded.request_id == result.request_id);
    CHECK(decoded.status == TranscriptionStatus::completed);
    CHECK(decoded.text == result.text);
    REQUIRE(decoded.segments.size() == 1);
    CHECK(decoded.segments.front().end_ms == 1840);
    CHECK(decoded.warnings == result.warnings);
    CHECK_FALSE(decoded.error.has_value());
}

TEST_CASE("TranscriptionResult omits unavailable optional metadata", "[runtime][protocol]") {
    TranscriptionResult result;
    result.request_id = "req-empty";
    result.status = TranscriptionStatus::no_speech;

    const nlohmann::json encoded = result;
    CHECK_FALSE(encoded.contains("language"));
    CHECK_FALSE(encoded.contains("duration_ms"));
    CHECK_FALSE(encoded.contains("processing_ms"));
    CHECK_FALSE(encoded.contains("model"));
    CHECK_FALSE(encoded.contains("error"));
    CHECK(encoded.at("segments").empty());
}

TEST_CASE("Failed transcription has a stable error envelope", "[runtime][protocol]") {
    const auto result = failed_transcription(
        "req-fail",
        "invalid_audio",
        "Audio could not be decoded",
        false
    );
    const nlohmann::json encoded = result;

    CHECK(encoded.at("status") == "failed");
    CHECK(encoded.at("text") == "");
    CHECK(encoded.at("error").at("code") == "invalid_audio");
    CHECK(encoded.at("error").at("retryable") == false);
}

TEST_CASE("Unknown TranscriptionResult versions fail closed", "[runtime][protocol]") {
    nlohmann::json encoded = {
        {"schema", "fabric.transcription"},
        {"version", 2},
        {"request_id", "future"},
        {"status", "completed"},
        {"text", "future"},
    };
    CHECK_THROWS_AS(encoded.get<TranscriptionResult>(), std::invalid_argument);
}

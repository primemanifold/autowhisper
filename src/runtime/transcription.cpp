#include "runtime/transcription.h"

#include <stdexcept>
#include <utility>

namespace autowhisper {

namespace {

TranscriptionStatus parse_status(const std::string& value) {
    if (value == "completed") return TranscriptionStatus::completed;
    if (value == "no_speech") return TranscriptionStatus::no_speech;
    if (value == "cancelled") return TranscriptionStatus::cancelled;
    if (value == "failed") return TranscriptionStatus::failed;
    throw std::invalid_argument("Unknown transcription status: " + value);
}

} // namespace

const char* transcription_status_name(TranscriptionStatus status) {
    switch (status) {
        case TranscriptionStatus::completed: return "completed";
        case TranscriptionStatus::no_speech: return "no_speech";
        case TranscriptionStatus::cancelled: return "cancelled";
        case TranscriptionStatus::failed: return "failed";
    }
    return "failed";
}

TranscriptionResult failed_transcription(
    std::string request_id,
    std::string code,
    std::string message,
    bool retryable
) {
    TranscriptionResult result;
    result.request_id = std::move(request_id);
    result.status = TranscriptionStatus::failed;
    result.error = TranscriptionError{
        std::move(code),
        std::move(message),
        retryable,
    };
    return result;
}

void to_json(nlohmann::json& json, const TranscriptionSegment& segment) {
    json = {
        {"start_ms", segment.start_ms},
        {"end_ms", segment.end_ms},
        {"text", segment.text},
    };
}

void from_json(const nlohmann::json& json, TranscriptionSegment& segment) {
    json.at("start_ms").get_to(segment.start_ms);
    json.at("end_ms").get_to(segment.end_ms);
    json.at("text").get_to(segment.text);
}

void to_json(nlohmann::json& json, const TranscriptionError& error) {
    json = {
        {"code", error.code},
        {"message", error.message},
        {"retryable", error.retryable},
    };
}

void from_json(const nlohmann::json& json, TranscriptionError& error) {
    json.at("code").get_to(error.code);
    json.at("message").get_to(error.message);
    json.at("retryable").get_to(error.retryable);
}

void to_json(nlohmann::json& json, const TranscriptionResult& result) {
    json = {
        {"schema", TRANSCRIPTION_SCHEMA},
        {"version", TRANSCRIPTION_VERSION},
        {"request_id", result.request_id},
        {"status", transcription_status_name(result.status)},
        {"text", result.text},
        {"provider", result.provider},
        {"segments", result.segments},
        {"warnings", result.warnings},
    };
    if (!result.language.empty()) json["language"] = result.language;
    if (result.duration_ms > 0) json["duration_ms"] = result.duration_ms;
    if (result.processing_ms > 0) json["processing_ms"] = result.processing_ms;
    if (!result.model.empty()) json["model"] = result.model;
    if (result.error.has_value()) json["error"] = *result.error;
}

void from_json(const nlohmann::json& json, TranscriptionResult& result) {
    if (json.at("schema").get<std::string>() != TRANSCRIPTION_SCHEMA) {
        throw std::invalid_argument("Unsupported transcription schema");
    }
    if (json.at("version").get<int>() != TRANSCRIPTION_VERSION) {
        throw std::invalid_argument("Unsupported transcription schema version");
    }
    json.at("request_id").get_to(result.request_id);
    result.status = parse_status(json.at("status").get<std::string>());
    json.at("text").get_to(result.text);
    result.provider = json.value("provider", "autowhisper");
    result.language = json.value("language", "");
    result.duration_ms = json.value("duration_ms", std::int64_t{0});
    result.processing_ms = json.value("processing_ms", std::int64_t{0});
    result.model = json.value("model", "");
    result.segments = json.value("segments", std::vector<TranscriptionSegment>{});
    result.warnings = json.value("warnings", std::vector<std::string>{});
    if (json.contains("error")) {
        result.error = json.at("error").get<TranscriptionError>();
    } else {
        result.error.reset();
    }
}

} // namespace autowhisper

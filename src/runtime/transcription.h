#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace autowhisper {

inline constexpr const char* TRANSCRIPTION_SCHEMA = "fabric.transcription";
inline constexpr int TRANSCRIPTION_VERSION = 1;

enum class TranscriptionStatus {
    completed,
    no_speech,
    cancelled,
    failed,
};

struct TranscriptionSegment {
    std::int64_t start_ms = 0;
    std::int64_t end_ms = 0;
    std::string text;
};

struct TranscriptionError {
    std::string code;
    std::string message;
    bool retryable = false;
};

struct TranscriptionResult {
    std::string request_id;
    TranscriptionStatus status = TranscriptionStatus::failed;
    std::string text;
    std::string language;
    std::int64_t duration_ms = 0;
    std::int64_t processing_ms = 0;
    std::string model;
    std::string provider = "autowhisper";
    std::vector<TranscriptionSegment> segments;
    std::vector<std::string> warnings;
    std::optional<TranscriptionError> error;
};

struct TranscriptionOptions {
    std::string request_id;
    std::string language;
    std::string model;
};

class Transcriber {
public:
    virtual ~Transcriber() = default;
    virtual TranscriptionResult transcribe_file(
        const std::string& path,
        const TranscriptionOptions& options
    ) = 0;
};

const char* transcription_status_name(TranscriptionStatus status);
TranscriptionResult failed_transcription(
    std::string request_id,
    std::string code,
    std::string message,
    bool retryable = false
);

void to_json(nlohmann::json& json, const TranscriptionSegment& segment);
void from_json(const nlohmann::json& json, TranscriptionSegment& segment);
void to_json(nlohmann::json& json, const TranscriptionError& error);
void from_json(const nlohmann::json& json, TranscriptionError& error);
void to_json(nlohmann::json& json, const TranscriptionResult& result);
void from_json(const nlohmann::json& json, TranscriptionResult& result);

} // namespace autowhisper

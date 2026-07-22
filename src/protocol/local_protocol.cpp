#include "protocol/local_protocol.h"

#include "runtime/audio_file.h"

#include <exception>
#include <utility>

namespace autowhisper {

namespace {

struct BoundedLine {
    std::string value;
    bool too_large = false;
    bool has_line = false;
};

BoundedLine read_bounded_line(std::istream& input) {
    BoundedLine line;
    line.value.reserve(4096);
    char character = '\0';
    while (input.get(character)) {
        line.has_line = true;
        if (character == '\n') return line;
        if (character == '\r') continue;
        if (line.value.size() < LOCAL_PROTOCOL_MAX_LINE_BYTES) {
            line.value.push_back(character);
        } else {
            line.too_large = true;
        }
    }
    return line;
}

bool valid_id(const nlohmann::json& id) {
    if (!id.is_string()) return false;
    const auto value = id.get<std::string>();
    return !value.empty() && value.size() <= 128;
}

std::string optional_bounded_string(
    const nlohmann::json& object,
    const char* key,
    std::size_t maximum
) {
    if (!object.contains(key)) return {};
    const auto& value = object.at(key);
    if (!value.is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    auto result = value.get<std::string>();
    if (result.size() > maximum) {
        throw std::invalid_argument(std::string(key) + " is too long");
    }
    return result;
}

} // namespace

ProtocolHandler::ProtocolHandler(Transcriber& transcriber)
    : transcriber_(transcriber) {}

nlohmann::json ProtocolHandler::success_response(
    const nlohmann::json& id,
    nlohmann::json result
) {
    return {
        {"protocol", LOCAL_PROTOCOL_NAME},
        {"version", LOCAL_PROTOCOL_VERSION},
        {"id", id},
        {"result", std::move(result)},
    };
}

nlohmann::json ProtocolHandler::error_response(
    const nlohmann::json& id,
    std::string code,
    std::string message,
    bool retryable
) {
    return {
        {"protocol", LOCAL_PROTOCOL_NAME},
        {"version", LOCAL_PROTOCOL_VERSION},
        {"id", id},
        {"error", {
            {"code", std::move(code)},
            {"message", std::move(message)},
            {"retryable", retryable},
        }},
    };
}

nlohmann::json ProtocolHandler::line_too_large_error() const {
    return error_response(
        nullptr,
        "request_too_large",
        "Protocol request exceeds the 1 MiB line limit",
        false
    );
}

nlohmann::json ProtocolHandler::handle_line(const std::string& line) {
    try {
        return handle_request(nlohmann::json::parse(line));
    } catch (const nlohmann::json::parse_error&) {
        return error_response(nullptr, "invalid_json", "Request is not valid JSON", false);
    } catch (const std::exception& error) {
        return error_response(nullptr, "invalid_request", error.what(), false);
    }
}

nlohmann::json ProtocolHandler::handle_request(const nlohmann::json& request) {
    if (!request.is_object()) {
        return error_response(nullptr, "invalid_request", "Request must be a JSON object");
    }

    const nlohmann::json id = request.value("id", nlohmann::json(nullptr));
    if (!valid_id(id)) {
        return error_response(nullptr, "invalid_id", "id must be a non-empty string of at most 128 bytes");
    }
    if (request.value("protocol", "") != LOCAL_PROTOCOL_NAME) {
        return error_response(id, "unsupported_protocol", "Unsupported local protocol name");
    }
    if (!request.contains("version") || !request.at("version").is_number_integer() ||
        request.at("version").get<int>() != LOCAL_PROTOCOL_VERSION) {
        return error_response(id, "unsupported_version", "Unsupported local protocol version");
    }
    if (!request.contains("method") || !request.at("method").is_string()) {
        return error_response(id, "invalid_method", "method must be a string");
    }
    const std::string method = request.at("method").get<std::string>();

    if (method == "health") {
        return success_response(id, {{"status", "ready"}});
    }
    if (method == "capabilities") {
        return success_response(id, {
            {"methods", {"health", "capabilities", "transcribe_file", "shutdown"}},
            {"input", {
                {"kind", "file"},
                {"formats", {"wav", "mp3", "flac"}},
                {"sample_rate", RUNTIME_SAMPLE_RATE},
                {"channels", 1},
            }},
            {"max_audio_seconds", MAX_TRANSCRIPTION_AUDIO_SECONDS},
            {"max_file_bytes", MAX_TRANSCRIPTION_FILE_BYTES},
        });
    }
    if (method == "shutdown") {
        shutdown_requested_ = true;
        return success_response(id, {{"status", "stopping"}});
    }
    if (method != "transcribe_file") {
        return error_response(id, "method_not_found", "Unknown method: " + method);
    }

    const nlohmann::json params = request.value("params", nlohmann::json::object());
    if (!params.is_object()) {
        return error_response(id, "invalid_params", "params must be a JSON object");
    }
    try {
        const std::string path = optional_bounded_string(params, "path", 32768);
        if (path.empty()) {
            return error_response(id, "invalid_params", "params.path is required");
        }
        TranscriptionOptions options;
        options.request_id = id.get<std::string>();
        options.language = optional_bounded_string(params, "language", 64);
        options.model = optional_bounded_string(params, "model", 128);
        TranscriptionResult result = transcriber_.transcribe_file(path, options);
        return success_response(id, result);
    } catch (const std::invalid_argument& error) {
        return error_response(id, "invalid_params", error.what());
    } catch (const std::exception& error) {
        return error_response(id, "runtime_error", error.what(), true);
    }
}

StdioServer::StdioServer(ProtocolHandler& handler)
    : handler_(handler) {}

int StdioServer::run(std::istream& input, std::ostream& output) {
    for (;;) {
        BoundedLine line = read_bounded_line(input);
        if (!line.has_line) return 0;
        nlohmann::json response = line.too_large
            ? handler_.line_too_large_error()
            : handler_.handle_line(line.value);
        output << response.dump() << '\n' << std::flush;
        if (handler_.shutdown_requested()) return 0;
        if (!input && !line.has_line) return 0;
    }
}

} // namespace autowhisper

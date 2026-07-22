#pragma once

#include "runtime/transcription.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <istream>
#include <ostream>
#include <string>

namespace autowhisper {

inline constexpr const char* LOCAL_PROTOCOL_NAME = "autowhisper.local";
inline constexpr int LOCAL_PROTOCOL_VERSION = 1;
inline constexpr std::size_t LOCAL_PROTOCOL_MAX_LINE_BYTES = 1024 * 1024;

class ProtocolHandler {
public:
    explicit ProtocolHandler(Transcriber& transcriber);

    nlohmann::json handle_line(const std::string& line);
    nlohmann::json line_too_large_error() const;
    bool shutdown_requested() const { return shutdown_requested_; }

private:
    Transcriber& transcriber_;
    bool shutdown_requested_ = false;

    nlohmann::json handle_request(const nlohmann::json& request);
    static nlohmann::json success_response(
        const nlohmann::json& id,
        nlohmann::json result
    );
    static nlohmann::json error_response(
        const nlohmann::json& id,
        std::string code,
        std::string message,
        bool retryable = false
    );
};

class StdioServer {
public:
    explicit StdioServer(ProtocolHandler& handler);
    int run(std::istream& input, std::ostream& output);

private:
    ProtocolHandler& handler_;
};

} // namespace autowhisper

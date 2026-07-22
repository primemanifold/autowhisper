#include <catch2/catch_test_macros.hpp>

#include "protocol/local_protocol.h"

#include <sstream>
#include <stdexcept>

using namespace autowhisper;

namespace {

class FakeTranscriber final : public Transcriber {
public:
    int calls = 0;
    std::string last_path;
    TranscriptionOptions last_options;
    bool should_throw = false;

    TranscriptionResult transcribe_file(
        const std::string& path,
        const TranscriptionOptions& options
    ) override {
        ++calls;
        last_path = path;
        last_options = options;
        if (should_throw) throw std::runtime_error("engine unavailable");

        TranscriptionResult result;
        result.request_id = options.request_id;
        result.status = TranscriptionStatus::completed;
        result.text = "Local transcription";
        result.language = options.language;
        result.model = options.model.empty() ? "small.en" : options.model;
        result.duration_ms = 500;
        result.segments.push_back({0, 500, result.text});
        return result;
    }
};

nlohmann::json request(
    std::string id,
    std::string method,
    nlohmann::json params = nlohmann::json::object()
) {
    return {
        {"protocol", LOCAL_PROTOCOL_NAME},
        {"version", LOCAL_PROTOCOL_VERSION},
        {"id", std::move(id)},
        {"method", std::move(method)},
        {"params", std::move(params)},
    };
}

} // namespace

TEST_CASE("Local protocol health and capabilities do not invoke the engine", "[protocol]") {
    FakeTranscriber transcriber;
    ProtocolHandler handler(transcriber);

    const auto health = handler.handle_line(request("h", "health").dump());
    CHECK(health.at("id") == "h");
    CHECK(health.at("result").at("status") == "ready");

    const auto capabilities = handler.handle_line(request("c", "capabilities").dump());
    CHECK(capabilities.at("result").at("methods").size() == 4);
    CHECK(capabilities.at("result").at("input").at("sample_rate") == 16000);
    CHECK(transcriber.calls == 0);
}

TEST_CASE("Local protocol correlates transcription request and result", "[protocol]") {
    FakeTranscriber transcriber;
    ProtocolHandler handler(transcriber);
    const auto response = handler.handle_line(request(
        "req-1",
        "transcribe_file",
        {{"path", "/tmp/audio.wav"}, {"language", "en"}, {"model", "small.en"}}
    ).dump());

    CHECK(response.at("id") == "req-1");
    CHECK(response.at("result").at("request_id") == "req-1");
    CHECK(response.at("result").at("status") == "completed");
    CHECK(response.at("result").at("text") == "Local transcription");
    CHECK(transcriber.calls == 1);
    CHECK(transcriber.last_path == "/tmp/audio.wav");
    CHECK(transcriber.last_options.language == "en");
}

TEST_CASE("Local protocol rejects malformed and incompatible requests", "[protocol]") {
    FakeTranscriber transcriber;
    ProtocolHandler handler(transcriber);

    CHECK(handler.handle_line("not json").at("error").at("code") == "invalid_json");
    CHECK(handler.handle_line("[]").at("error").at("code") == "invalid_request");

    auto incompatible = request("old", "health");
    incompatible["version"] = 99;
    const auto response = handler.handle_line(incompatible.dump());
    CHECK(response.at("id") == "old");
    CHECK(response.at("error").at("code") == "unsupported_version");

    const auto unknown = handler.handle_line(request("x", "missing").dump());
    CHECK(unknown.at("error").at("code") == "method_not_found");
    CHECK(transcriber.calls == 0);
}

TEST_CASE("Local protocol converts engine exceptions into retryable errors", "[protocol]") {
    FakeTranscriber transcriber;
    transcriber.should_throw = true;
    ProtocolHandler handler(transcriber);
    const auto response = handler.handle_line(request(
        "req-error",
        "transcribe_file",
        {{"path", "/tmp/audio.wav"}}
    ).dump());

    CHECK(response.at("id") == "req-error");
    CHECK(response.at("error").at("code") == "runtime_error");
    CHECK(response.at("error").at("retryable") == true);
}

TEST_CASE("Stdio service emits one response per line and stops on shutdown", "[protocol]") {
    FakeTranscriber transcriber;
    ProtocolHandler handler(transcriber);
    StdioServer server(handler);
    std::istringstream input(
        request("h", "health").dump() + "\n" +
        request("s", "shutdown").dump() + "\n" +
        request("ignored", "health").dump() + "\n"
    );
    std::ostringstream output;

    CHECK(server.run(input, output) == 0);
    std::istringstream responses(output.str());
    std::string first;
    std::string second;
    std::string third;
    REQUIRE(static_cast<bool>(std::getline(responses, first)));
    REQUIRE(static_cast<bool>(std::getline(responses, second)));
    CHECK_FALSE(static_cast<bool>(std::getline(responses, third)));
    CHECK(nlohmann::json::parse(first).at("id") == "h");
    CHECK(nlohmann::json::parse(second).at("result").at("status") == "stopping");
}

TEST_CASE("Stdio service bounds request lines before JSON parsing", "[protocol]") {
    FakeTranscriber transcriber;
    ProtocolHandler handler(transcriber);
    StdioServer server(handler);
    std::istringstream input(std::string(LOCAL_PROTOCOL_MAX_LINE_BYTES + 1, 'x') + "\n");
    std::ostringstream output;

    CHECK(server.run(input, output) == 0);
    const auto response = nlohmann::json::parse(output.str());
    CHECK(response.at("error").at("code") == "request_too_large");
    CHECK(transcriber.calls == 0);
}

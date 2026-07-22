#include <catch2/catch_test_macros.hpp>

#include "fabric/fabric_client.h"

using namespace autowhisper;

TEST_CASE("FabricClient sends private prompts over stdin", "[fabric]") {
    FabricConfig config;
    config.executable = "/custom/fabric";
    config.timeout_seconds = 42;

    std::vector<std::string> seen_args;
    std::string seen_input;
    int seen_timeout = 0;
    std::size_t seen_limit = 0;
    FabricClient client(config,
        [&](const auto& args, const auto& input, int timeout, std::size_t limit) {
            seen_args = args;
            seen_input = input;
            seen_timeout = timeout;
            seen_limit = limit;
            ProcessResult result;
            result.exit_code = 0;
            result.stdout_str = "  Fabric answer\n";
            return result;
        });

    auto result = client.ask("private spoken question");

    REQUIRE(result.ok());
    CHECK(result.response == "Fabric answer");
    CHECK(seen_args == std::vector<std::string>{
        "/custom/fabric", "--oneshot-stdin", "--toolsets", "safe"});
    CHECK(seen_input == "private spoken question");
    CHECK(seen_timeout == 42);
    CHECK(seen_limit == FabricClient::MAX_RESPONSE_BYTES);
}

TEST_CASE("FabricClient fails closed without exposing process diagnostics", "[fabric]") {
    FabricConfig config;
    FabricClient client(config, [](const auto&, const auto&, int, std::size_t) {
        ProcessResult result;
        result.exit_code = 7;
        result.stderr_str = "provider diagnostic containing private data";
        return result;
    });

    auto result = client.ask("private prompt");

    CHECK_FALSE(result.ok());
    CHECK(result.status == FabricAskStatus::PROCESS_FAILED);
    CHECK(result.error == "Fabric exited with code 7.");
    CHECK(result.error.find("private") == std::string::npos);
}

TEST_CASE("FabricClient rejects timeout, oversized, and empty responses", "[fabric]") {
    FabricConfig config;

    SECTION("timeout") {
        FabricClient client(config, [](const auto&, const auto&, int, std::size_t) {
            ProcessResult result;
            result.exit_code = -1;
            result.stderr_str = "timeout";
            return result;
        });
        CHECK(client.ask("question").status == FabricAskStatus::TIMED_OUT);
    }

    SECTION("oversized response") {
        FabricClient client(config, [](const auto&, const auto&, int, std::size_t) {
            ProcessResult result;
            result.exit_code = 0;
            result.output_truncated = true;
            return result;
        });
        CHECK(client.ask("question").status == FabricAskStatus::RESPONSE_TOO_LARGE);
    }

    SECTION("empty response") {
        FabricClient client(config, [](const auto&, const auto&, int, std::size_t) {
            ProcessResult result;
            result.exit_code = 0;
            result.stdout_str = "  \n";
            return result;
        });
        CHECK(client.ask("question").status == FabricAskStatus::EMPTY_RESPONSE);
    }
}

TEST_CASE("FabricClient rejects invalid prompts before spawning", "[fabric]") {
    FabricConfig config;
    bool called = false;
    FabricClient client(config, [&](const auto&, const auto&, int, std::size_t) {
        called = true;
        return ProcessResult{};
    });

    CHECK(client.ask("").status == FabricAskStatus::INVALID_PROMPT);
    CHECK(client.ask(std::string(FabricClient::MAX_PROMPT_BYTES + 1, 'x')).status ==
          FabricAskStatus::INVALID_PROMPT);
    CHECK_FALSE(called);
}

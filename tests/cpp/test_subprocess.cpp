#include "util/subprocess.h"

#include <catch2/catch_test_macros.hpp>

#ifndef _WIN32

TEST_CASE("run_command_with_input echoes stdin to cat", "[subprocess]") {
    const std::string input = "hello\nworld\n";
    auto result = autowhisper::run_command_with_input({"cat"}, input, 5);

    REQUIRE(result.exit_code == 0);
    REQUIRE(result.stdout_str == input);
}

TEST_CASE("run_command_with_input enforces timeout", "[subprocess]") {
    auto result = autowhisper::run_command_with_input({"sleep", "2"}, "ignored", 1);

    REQUIRE(result.exit_code == -1);
    REQUIRE(result.stderr_str == "timeout");
}

TEST_CASE("run_command enforces wall-clock timeout for noisy processes", "[subprocess]") {
    auto result = autowhisper::run_command(
        {"bash", "-lc", "for i in $(seq 1 40); do echo tick; sleep 0.1; done"},
        1);

    REQUIRE(result.exit_code == -1);
    REQUIRE(result.stderr_str == "timeout");
}

TEST_CASE("run_command_with_input times out when child never drains stdin", "[subprocess]") {
    std::string large_input(2 * 1024 * 1024, 'x');
    auto result = autowhisper::run_command_with_input({"sleep", "2"}, large_input, 1);

    REQUIRE(result.exit_code == -1);
    REQUIRE(result.stderr_str == "timeout");
}

TEST_CASE("subprocess capture is bounded while the child is fully drained", "[subprocess]") {
    auto result = autowhisper::run_command(
        {"bash", "-lc", "printf '%0200d' 0; printf '%0200d' 0 >&2"}, 5, 64);

    REQUIRE(result.exit_code == 0);
    CHECK(result.output_truncated);
    CHECK(result.stdout_str.size() + result.stderr_str.size() == 64);
}

#endif

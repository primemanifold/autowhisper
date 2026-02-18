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

#endif

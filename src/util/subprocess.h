#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace autowhisper {

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_str;
    std::string stderr_str;
    bool output_truncated = false;
};

// Run a command and capture output
ProcessResult run_command(
    const std::vector<std::string>& args,
    int timeout_seconds = 10,
    std::size_t max_output_bytes = std::numeric_limits<std::size_t>::max());

// Run a command with stdin input
ProcessResult run_command_with_input(const std::vector<std::string>& args,
                                     const std::string& input,
                                     int timeout_seconds = 10,
                                     std::size_t max_output_bytes =
                                         std::numeric_limits<std::size_t>::max());

// Check if a command exists on PATH
bool command_exists(const std::string& name);

// Run a command without capturing output (passes through to terminal)
int run_passthrough(const std::vector<std::string>& args);

} // namespace autowhisper

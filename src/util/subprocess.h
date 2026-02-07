#pragma once

#include <string>
#include <vector>
#include <optional>

namespace autowhisper {

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_str;
    std::string stderr_str;
};

// Run a command and capture output
ProcessResult run_command(const std::vector<std::string>& args, int timeout_seconds = 10);

// Run a command with stdin input
ProcessResult run_command_with_input(const std::vector<std::string>& args,
                                     const std::string& input,
                                     int timeout_seconds = 10);

// Check if a command exists on PATH
bool command_exists(const std::string& name);

// Run a command without capturing output (passes through to terminal)
int run_passthrough(const std::vector<std::string>& args);

} // namespace autowhisper

#include "fabric/fabric_client.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace autowhisper {

namespace {

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

ProcessResult default_runner(const std::vector<std::string>& args,
                             const std::string& input,
                             int timeout_seconds,
                             std::size_t max_output_bytes) {
    return run_command_with_input(args, input, timeout_seconds, max_output_bytes);
}

}  // namespace

FabricClient::FabricClient(FabricConfig config, Runner runner)
    : config_(std::move(config)),
      runner_(runner ? std::move(runner) : Runner(default_runner)) {}

FabricAskResult FabricClient::ask(const std::string& prompt) const {
    if (prompt.empty() || prompt.size() > MAX_PROMPT_BYTES) {
        return {FabricAskStatus::INVALID_PROMPT, {},
                prompt.empty() ? "The transcript was empty."
                               : "The transcript exceeded the 1 MiB limit."};
    }

    ProcessResult process = runner_(
        {config_.executable, "--oneshot-stdin", "--toolsets", "safe"},
        prompt,
        config_.timeout_seconds,
        MAX_RESPONSE_BYTES);

    if (process.output_truncated) {
        return {FabricAskStatus::RESPONSE_TOO_LARGE, {},
                "Fabric returned more than 256 KiB."};
    }
    if (process.exit_code == -1 && process.stderr_str == "timeout") {
        return {FabricAskStatus::TIMED_OUT, {}, "Fabric did not respond before the timeout."};
    }
    if (process.exit_code != 0) {
        return {FabricAskStatus::PROCESS_FAILED, {},
                "Fabric exited with code " + std::to_string(process.exit_code) + "."};
    }

    std::string response = trim(std::move(process.stdout_str));
    if (response.empty()) {
        return {FabricAskStatus::EMPTY_RESPONSE, {}, "Fabric returned an empty response."};
    }
    return {FabricAskStatus::SUCCESS, std::move(response), {}};
}

}  // namespace autowhisper

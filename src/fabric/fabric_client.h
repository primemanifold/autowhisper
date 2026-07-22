#pragma once

#include "config/config.h"
#include "util/subprocess.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace autowhisper {

enum class FabricAskStatus {
    SUCCESS,
    INVALID_PROMPT,
    PROCESS_FAILED,
    TIMED_OUT,
    RESPONSE_TOO_LARGE,
    EMPTY_RESPONSE,
};

struct FabricAskResult {
    FabricAskStatus status = FabricAskStatus::PROCESS_FAILED;
    std::string response;
    std::string error;

    bool ok() const { return status == FabricAskStatus::SUCCESS; }
};

class FabricClient {
public:
    static constexpr std::size_t MAX_PROMPT_BYTES = 1024 * 1024;
    static constexpr std::size_t MAX_RESPONSE_BYTES = 256 * 1024;

    using Runner = std::function<ProcessResult(
        const std::vector<std::string>&,
        const std::string&,
        int,
        std::size_t)>;

    explicit FabricClient(FabricConfig config, Runner runner = {});
    FabricAskResult ask(const std::string& prompt) const;

private:
    FabricConfig config_;
    Runner runner_;
};

}  // namespace autowhisper

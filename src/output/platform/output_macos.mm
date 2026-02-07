// macOS output implementation placeholder
#include "output/output.h"
#include <spdlog/spdlog.h>

namespace autowhisper {

struct OutputManager::Impl {};

OutputManager::OutputManager(const OutputConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {}

OutputManager::~OutputManager() = default;

bool OutputManager::inject_platform(const std::string& text) {
    spdlog::warn("macOS text injection not yet implemented");
    return false;
}

} // namespace autowhisper

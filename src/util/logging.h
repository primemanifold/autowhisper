#pragma once

#include <string>
#include <spdlog/spdlog.h>

namespace autowhisper {

void setup_logging(const std::string& level, const std::string& log_file = "");

} // namespace autowhisper

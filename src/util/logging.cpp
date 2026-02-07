#include "util/logging.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <vector>

namespace autowhisper {

void setup_logging(const std::string& level, const std::string& log_file) {
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks{console_sink};

    if (!log_file.empty()) {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, false);
            sinks.push_back(file_sink);
        } catch (const spdlog::spdlog_ex& ex) {
            spdlog::warn("Could not open log file {}: {}", log_file, ex.what());
        }
    }

    auto logger = std::make_shared<spdlog::logger>("autowhisper", sinks.begin(), sinks.end());
    logger->set_pattern("%Y-%m-%d %H:%M:%S [%^%l%$] %n: %v");

    // Map level string
    spdlog::level::level_enum log_level = spdlog::level::info;
    if (level == "trace") log_level = spdlog::level::trace;
    else if (level == "debug") log_level = spdlog::level::debug;
    else if (level == "info") log_level = spdlog::level::info;
    else if (level == "warn" || level == "warning") log_level = spdlog::level::warn;
    else if (level == "error") log_level = spdlog::level::err;

    logger->set_level(log_level);
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::warn);
}

} // namespace autowhisper

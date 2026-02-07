#pragma once

#include "config/config.h"

#include <memory>
#include <string>

namespace autowhisper {

class OutputManager {
public:
    explicit OutputManager(const OutputConfig& config);
    ~OutputManager();

    OutputManager(const OutputManager&) = delete;
    OutputManager& operator=(const OutputManager&) = delete;

    void initialize();
    bool inject(const std::string& text);

private:
    OutputConfig config_;
    bool xdotool_available_ = false;
    bool xclip_available_ = false;

    // Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool inject_platform(const std::string& text);
    bool inject_xdotool(const std::string& text);
    bool copy_to_clipboard(const std::string& text);
    bool inject_clipboard(const std::string& text);
    bool send_paste();
    bool send_return_key();
};

} // namespace autowhisper

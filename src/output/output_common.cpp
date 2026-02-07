#include "output/output.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <thread>
#include <chrono>

namespace autowhisper {

void OutputManager::initialize() {
    xdotool_available_ = command_exists("xdotool");
    xclip_available_ = command_exists("xclip");

    spdlog::info("Output methods: xdotool={}, xclip={}", xdotool_available_, xclip_available_);
}

bool OutputManager::inject(const std::string& text) {
    if (text.empty()) {
        spdlog::debug("Empty text, skipping injection");
        return true;
    }

    // Apply transformations
    std::string processed = text;

    if (config_.lowercase) {
        std::transform(processed.begin(), processed.end(), processed.begin(), ::tolower);
    }

    if (config_.ending_action == "newline") {
        processed += "\n";
    }

    // Try injection methods
    if (config_.method == "inject") {
        // Also copy to clipboard if enabled
        if (config_.also_copy_to_clipboard) {
            copy_to_clipboard(processed);
        }

        // Try platform-native injection first
        if (inject_platform(processed)) {
            if (config_.ending_action == "return_key") {
                send_return_key();
            }
            return true;
        }

        // Fall back to xdotool
        if (xdotool_available_ && inject_xdotool(processed)) {
            if (config_.ending_action == "return_key") {
                send_return_key();
            }
            return true;
        }

        spdlog::warn("Direct injection failed, falling back to clipboard");
    }

    // Clipboard method (or fallback)
    bool result = inject_clipboard(processed);
    if (result && config_.ending_action == "return_key") {
        send_return_key();
    }
    return result;
}

bool OutputManager::inject_xdotool(const std::string& text) {
    auto result = run_command({"xdotool", "type", "--clearmodifiers", "--", text}, 10);
    if (result.exit_code == 0) {
        spdlog::debug("Injected {} characters via xdotool", text.size());
        return true;
    }
    spdlog::warn("xdotool failed: {}", result.stderr_str);
    return false;
}

bool OutputManager::copy_to_clipboard(const std::string& text) {
    if (xclip_available_) {
        auto result = run_command_with_input({"xclip", "-selection", "clipboard"}, text, 5);
        if (result.exit_code == 0) {
            spdlog::debug("Copied {} characters to clipboard", text.size());
            return true;
        }
        spdlog::warn("xclip failed");
    } else {
        // Try xsel as fallback
        auto result = run_command_with_input({"xsel", "--clipboard", "--input"}, text, 5);
        if (result.exit_code == 0) {
            spdlog::debug("Copied {} characters to clipboard via xsel", text.size());
            return true;
        }
    }
    spdlog::warn("Clipboard copy failed");
    return false;
}

bool OutputManager::inject_clipboard(const std::string& text) {
    if (!copy_to_clipboard(text)) return false;

    if (config_.auto_paste) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(config_.paste_delay * 1000)));
        return send_paste();
    }
    return true;
}

bool OutputManager::send_paste() {
    if (xdotool_available_) {
        auto result = run_command({"xdotool", "key", "--clearmodifiers", "ctrl+v"}, 5);
        return result.exit_code == 0;
    }
    spdlog::warn("Cannot send paste: xdotool not available");
    return false;
}

bool OutputManager::send_return_key() {
    if (xdotool_available_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto result = run_command({"xdotool", "key", "--clearmodifiers", "Return"}, 5);
        if (result.exit_code == 0) {
            spdlog::debug("Sent Return keypress via xdotool");
            return true;
        }
        spdlog::warn("xdotool key Return failed: {}", result.stderr_str);
    }
    spdlog::warn("Cannot send Return key: xdotool not available");
    return false;
}

} // namespace autowhisper

#include "output/output.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace autowhisper {

OutputManager::OutputManager(const OutputConfig& config)
    : config_(config), platform_(make_platform_output()) {}

OutputManager::OutputManager(const OutputConfig& config,
                             std::unique_ptr<PlatformOutput> platform)
    : config_(config), platform_(std::move(platform)) {}

OutputManager::~OutputManager() = default;

void OutputManager::initialize() {
    if (platform_) {
        platform_->initialize();
        spdlog::info("Output platform: {} (post_events={}, clipboard={})",
                     platform_->name(),
                     platform_->can_post_events(),
                     platform_->can_copy_to_clipboard());
    } else {
        spdlog::error("Output: no platform implementation available");
    }
}

bool OutputManager::inject(const std::string& text) {
    if (text.empty()) {
        spdlog::debug("Empty text, skipping injection");
        last_outcome_ = Outcome::NOOP_EMPTY;
        return true;
    }

    if (!platform_) {
        last_outcome_ = Outcome::FAILED;
        return false;
    }

    std::string processed = text;
    if (config_.lowercase) {
        std::transform(processed.begin(), processed.end(),
                       processed.begin(), ::tolower);
    }
    if (config_.ending_action == "newline") {
        processed += "\n";
    }

    const bool post_events = platform_->can_post_events();

    // Strategy:
    //   1) method=="inject" + post_events: try platform inject; if it fails,
    //      fall through to clipboard path.
    //   2) method=="inject" + !post_events: degraded — clipboard only (no
    //      auto-paste, no return-key — they would all fail).
    //   3) method=="clipboard": clipboard path as before.
    //
    // Rationale (codex #4): when post-event access is denied, direct inject,
    // cmd+v paste, and synthesized return-key all share the same permission.
    // The only real degraded mode is "copy to clipboard and tell the user."

    if (config_.method == "inject" && post_events) {
        if (config_.also_copy_to_clipboard && platform_->can_copy_to_clipboard()) {
            platform_->copy_to_clipboard(processed);
        }

        if (deliver_inject(processed)) return true;

        spdlog::warn("Platform inject failed; falling back to clipboard");
    }

    // Clipboard path (degraded or explicitly requested).
    return deliver_clipboard(processed, post_events);
}

bool OutputManager::deliver_inject(const std::string& text) {
    if (!platform_->inject(text)) {
        return false;
    }
    if (config_.ending_action == "return_key") {
        platform_->send_return_key();
    }
    last_outcome_ = Outcome::INJECTED;
    return true;
}

bool OutputManager::deliver_clipboard(const std::string& text, bool post_events_allowed) {
    if (!platform_->can_copy_to_clipboard()) {
        spdlog::warn("Clipboard not available on this platform");
        last_outcome_ = Outcome::FAILED;
        return false;
    }

    if (!platform_->copy_to_clipboard(text)) {
        spdlog::warn("Clipboard copy failed");
        last_outcome_ = Outcome::FAILED;
        return false;
    }

    if (!config_.auto_paste || !post_events_allowed) {
        // Degraded mode: copied only. The caller/daemon is responsible for
        // surfacing this to the user via tray state + feedback tone.
        spdlog::info("Transcript copied to clipboard (no auto-paste: "
                     "auto_paste={}, post_events_allowed={})",
                     config_.auto_paste, post_events_allowed);
        last_outcome_ = Outcome::CLIPBOARD_ONLY;
        return true;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(config_.paste_delay * 1000)));

    if (!platform_->send_paste()) {
        spdlog::warn("Auto-paste failed after clipboard copy");
        last_outcome_ = Outcome::CLIPBOARD_ONLY;
        return false;
    }

    if (config_.ending_action == "return_key") {
        platform_->send_return_key();
    }

    last_outcome_ = Outcome::CLIPBOARD_PASTED;
    return true;
}

} // namespace autowhisper

#pragma once

#include "config/config.h"
#include "output/platform_output.h"

#include <memory>
#include <string>

namespace autowhisper {

class OutputManager {
public:
    // Production ctor: owns a real PlatformOutput from make_platform_output().
    explicit OutputManager(const OutputConfig& config);

    // Test ctor: inject a fake PlatformOutput.
    OutputManager(const OutputConfig& config,
                  std::unique_ptr<PlatformOutput> platform);

    ~OutputManager();

    OutputManager(const OutputManager&) = delete;
    OutputManager& operator=(const OutputManager&) = delete;

    void initialize();
    bool inject(const std::string& text);

    // Exposed for observability; returns the last outcome of inject().
    enum class Outcome {
        INJECTED,          // Direct synthesized-input typing succeeded.
        CLIPBOARD_PASTED,  // Clipboard + paste succeeded.
        CLIPBOARD_ONLY,    // Copied to clipboard, no auto-paste (degraded mode).
        FAILED,            // Could not deliver text.
        NOOP_EMPTY,        // text was empty.
    };
    Outcome last_outcome() const { return last_outcome_; }

private:
    OutputConfig config_;
    std::unique_ptr<PlatformOutput> platform_;
    Outcome last_outcome_ = Outcome::NOOP_EMPTY;

    bool deliver_inject(const std::string& text);
    bool deliver_clipboard(const std::string& text, bool post_events_allowed);
};

} // namespace autowhisper

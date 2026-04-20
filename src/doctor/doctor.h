#pragma once

#include <string>
#include <vector>

namespace autowhisper {

enum class CheckStatus { OK, WARN, FAIL, INFO };

struct CheckResult {
    CheckStatus status = CheckStatus::OK;
    std::string label;
    std::string message;
    std::string fix;   // Optional: one-line recommended fix.
};

struct CheckGroup {
    std::string header;       // Section header, e.g. "GPU"
    std::vector<CheckResult> results;
};

// Platform-defined: emit the OS-specific check groups.
// Lives in doctor_{linux,macos}.cpp/mm.
std::vector<CheckGroup> platform_checks(bool fix);

// Platform-independent: emit the cross-platform check groups
// (currently: audio devices via miniaudio, model availability, config).
// Lives in doctor_common.cpp.
std::vector<CheckGroup> common_checks();

// Render a set of groups to stdout, compute totals, and return the exit
// code (number of failures). Used by `autowhisper doctor`.
int render_and_exit(const std::vector<CheckGroup>& groups);

// Top-level entry point used by cli.cpp.
int run_diagnostics(bool fix = false);

} // namespace autowhisper

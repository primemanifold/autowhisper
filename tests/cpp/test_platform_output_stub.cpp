// Stub for tests: provides a trivial make_platform_output() so the test
// target can link autowhisper_core (which contains output_common.cpp) without
// dragging in X11/Cocoa.
// Real tests should use the test ctor that injects a PlatformOutput directly.

#include "output/platform_output.h"

#include <stdexcept>

namespace autowhisper {

std::unique_ptr<PlatformOutput> make_platform_output() {
    // Intentionally returns nullptr — tests must use the injecting ctor.
    return nullptr;
}

} // namespace autowhisper

// Windows output implementation placeholder.
//
// The desktop-platform foundation proves Windows buildability before claiming
// runtime readiness. Keep this PlatformOutput implementation honest: it builds
// and reports unsupported delivery capabilities until a real Win32 text
// injection/clipboard implementation is validated on Windows.
#include "output/platform_output.h"

#include <memory>
#include <string>

namespace autowhisper {

namespace {

class Win32Output : public PlatformOutput {
public:
    bool inject(const std::string&) override { return false; }
    bool copy_to_clipboard(const std::string&) override { return false; }
    bool send_paste() override { return false; }
    bool send_return_key() override { return false; }

    bool can_post_events() const override { return false; }
    bool can_copy_to_clipboard() const override { return false; }

    std::string name() const override { return "win32-placeholder"; }
};

} // namespace

std::unique_ptr<PlatformOutput> make_platform_output() {
    return std::make_unique<Win32Output>();
}

} // namespace autowhisper

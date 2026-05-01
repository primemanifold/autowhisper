#pragma once

#include <memory>
#include <string>

namespace autowhisper {

class PlatformOutput {
public:
    virtual ~PlatformOutput() = default;

    virtual void initialize() {}

    virtual bool inject(const std::string& text) = 0;
    virtual bool copy_to_clipboard(const std::string& text) = 0;
    virtual bool send_paste() = 0;
    virtual bool send_return_key() = 0;

    virtual bool can_post_events() const = 0;
    virtual bool can_copy_to_clipboard() const = 0;

    virtual std::string name() const = 0;
};

std::unique_ptr<PlatformOutput> make_platform_output();

} // namespace autowhisper

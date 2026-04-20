#include "output/platform_output.h"

#include <spdlog/spdlog.h>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

#include <string>
#include <vector>

namespace autowhisper {

namespace {

// Synthesize a single key-down/key-up pair. If `virtual_key` is kVK-style
// keycode, the event is interpreted as that physical key; to type arbitrary
// Unicode we use virtual_key=0 and CGEventKeyboardSetUnicodeString.
void post_virtual_key(CGKeyCode vk, CGEventFlags flags) {
    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, vk, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(nullptr, vk, false);
    if (flags) {
        CGEventSetFlags(down, flags);
        CGEventSetFlags(up, flags);
    }
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down);
    CFRelease(up);
}

void post_unicode_chunk(const UniChar* utf16, size_t len) {
    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(nullptr, 0, false);
    CGEventKeyboardSetUnicodeString(down, len, utf16);
    CGEventKeyboardSetUnicodeString(up,   len, utf16);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down);
    CFRelease(up);
}

class MacOutput : public PlatformOutput {
public:
    void initialize() override {
        can_post_  = CGPreflightPostEventAccess();
        spdlog::info("macOS output: post_events={}", can_post_);
    }

    bool inject(const std::string& text) override {
        if (!can_post_) return false;

        NSString* ns = [NSString stringWithUTF8String:text.c_str()];
        if (!ns) return false;

        NSUInteger total = [ns length];
        const NSUInteger CHUNK = 20;  // CGEventKeyboardSetUnicodeString is bounded.
        std::vector<UniChar> buf(CHUNK);

        for (NSUInteger i = 0; i < total; ) {
            NSUInteger n = std::min<NSUInteger>(CHUNK, total - i);
            // Don't split a surrogate pair across chunks: if the last unit
            // in this window is a high surrogate (0xD800..0xDBFF) and
            // there's more text after, shrink the window by one so the
            // pair is emitted together on the next iteration.
            if (i + n < total && n > 0) {
                UniChar last = [ns characterAtIndex:(i + n - 1)];
                if (last >= 0xD800 && last <= 0xDBFF) {
                    n -= 1;
                }
            }
            [ns getCharacters:buf.data() range:NSMakeRange(i, n)];
            post_unicode_chunk(buf.data(), n);
            i += n;
        }

        spdlog::debug("Injected {} UTF-16 units via CGEventPost", total);
        return true;
    }

    bool copy_to_clipboard(const std::string& text) override {
        NSString* ns = [NSString stringWithUTF8String:text.c_str()];
        if (!ns) return false;

        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        BOOL ok = [pb setString:ns forType:NSPasteboardTypeString];
        if (ok) {
            spdlog::debug("Copied {} chars to NSPasteboard", text.size());
            return true;
        }
        spdlog::warn("NSPasteboard setString failed");
        return false;
    }

    bool send_paste() override {
        if (!can_post_) return false;
        post_virtual_key(kVK_ANSI_V, kCGEventFlagMaskCommand);
        return true;
    }

    bool send_return_key() override {
        if (!can_post_) return false;
        post_virtual_key(kVK_Return, 0);
        return true;
    }

    bool can_post_events() const override { return can_post_; }
    bool can_copy_to_clipboard() const override { return true; }

    std::string name() const override { return "macos"; }

private:
    bool can_post_ = false;
};

} // namespace

std::unique_ptr<PlatformOutput> make_platform_output() {
    return std::make_unique<MacOutput>();
}

} // namespace autowhisper

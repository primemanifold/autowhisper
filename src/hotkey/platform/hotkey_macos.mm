#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

#include <atomic>
#include <string>
#include <thread>

namespace autowhisper {

// Exposed for unit testing — pure function, no CGEventTap state.
// Returns the canonical hotkey name for a given macOS virtual keycode,
// matching the names used by the X11 backend (see hotkey_x11.cpp).
// Empty string means "we don't care about this key".
std::string macos_keycode_to_name(uint16_t vk) {
    switch (vk) {
        // Letters
        case kVK_ANSI_A: return "a";
        case kVK_ANSI_B: return "b";
        case kVK_ANSI_C: return "c";
        case kVK_ANSI_D: return "d";
        case kVK_ANSI_E: return "e";
        case kVK_ANSI_F: return "f";
        case kVK_ANSI_G: return "g";
        case kVK_ANSI_H: return "h";
        case kVK_ANSI_I: return "i";
        case kVK_ANSI_J: return "j";
        case kVK_ANSI_K: return "k";
        case kVK_ANSI_L: return "l";
        case kVK_ANSI_M: return "m";
        case kVK_ANSI_N: return "n";
        case kVK_ANSI_O: return "o";
        case kVK_ANSI_P: return "p";
        case kVK_ANSI_Q: return "q";
        case kVK_ANSI_R: return "r";
        case kVK_ANSI_S: return "s";
        case kVK_ANSI_T: return "t";
        case kVK_ANSI_U: return "u";
        case kVK_ANSI_V: return "v";
        case kVK_ANSI_W: return "w";
        case kVK_ANSI_X: return "x";
        case kVK_ANSI_Y: return "y";
        case kVK_ANSI_Z: return "z";
        // Digits (top-row)
        case kVK_ANSI_0: return "0";
        case kVK_ANSI_1: return "1";
        case kVK_ANSI_2: return "2";
        case kVK_ANSI_3: return "3";
        case kVK_ANSI_4: return "4";
        case kVK_ANSI_5: return "5";
        case kVK_ANSI_6: return "6";
        case kVK_ANSI_7: return "7";
        case kVK_ANSI_8: return "8";
        case kVK_ANSI_9: return "9";
        // Navigation + whitespace
        case kVK_Space:        return "space";
        case kVK_Return:       return "return";
        case kVK_Tab:          return "tab";
        case kVK_Escape:       return "esc";
        case kVK_Delete:       return "backspace";
        case kVK_ForwardDelete:return "delete";
        case kVK_Home:         return "home";
        case kVK_End:          return "end";
        case kVK_PageUp:       return "pageup";
        case kVK_PageDown:     return "pagedown";
        case kVK_LeftArrow:    return "left";
        case kVK_RightArrow:   return "right";
        case kVK_UpArrow:      return "up";
        case kVK_DownArrow:    return "down";
        // Function keys
        case kVK_F1:  return "f1";
        case kVK_F2:  return "f2";
        case kVK_F3:  return "f3";
        case kVK_F4:  return "f4";
        case kVK_F5:  return "f5";
        case kVK_F6:  return "f6";
        case kVK_F7:  return "f7";
        case kVK_F8:  return "f8";
        case kVK_F9:  return "f9";
        case kVK_F10: return "f10";
        case kVK_F11: return "f11";
        case kVK_F12: return "f12";
        default: return "";
    }
}

// Exposed for unit testing — reports the modifier name for a given
// CGEventFlags bit, matching the X11 backend names.
std::string macos_flag_bit_to_modifier(uint64_t bit) {
    switch (bit) {
        case kCGEventFlagMaskShift:      return "shift";
        case kCGEventFlagMaskControl:    return "ctrl";
        case kCGEventFlagMaskAlternate:  return "alt";
        case kCGEventFlagMaskCommand:    return "super";
        default: return "";
    }
}

struct HotkeyManager::Impl {
    HotkeyManager* manager = nullptr;
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef src = nullptr;
    CFRunLoopRef run_loop = nullptr;
    std::thread listener;
    CGEventFlags last_flags = 0;

    static CGEventRef tap_cb(CGEventTapProxy, CGEventType type,
                             CGEventRef ev, void* userdata);
};

CGEventRef HotkeyManager::Impl::tap_cb(CGEventTapProxy, CGEventType type,
                                        CGEventRef ev, void* userdata) {
    auto* impl = static_cast<Impl*>(userdata);
    if (!impl) return ev;

    // Handle tap-disabled events — required by Apple docs. Re-enable the tap
    // AND resync cached modifier state: while the tap was dead, modifier
    // presses/releases we would have seen may have happened. Clearing the
    // cache means the next real flagsChanged event correctly diffs 0 →
    // current-mask and emits press events for whatever is actually held.
    if (type == kCGEventTapDisabledByTimeout ||
        type == kCGEventTapDisabledByUserInput) {
        spdlog::warn("CGEventTap disabled ({}); re-enabling + resyncing state",
                     type == kCGEventTapDisabledByTimeout ? "timeout" : "user-input");
        if (impl->tap) CGEventTapEnable(impl->tap, true);
        impl->last_flags = 0;
        if (impl->manager) impl->manager->reset_input_state();
        return nullptr;
    }

    if (!impl->manager || !impl->manager->running_.load()) return ev;

    if (type == kCGEventFlagsChanged) {
        CGEventFlags flags = CGEventGetFlags(ev);
        constexpr CGEventFlags kWatchedBits[] = {
            kCGEventFlagMaskShift,
            kCGEventFlagMaskControl,
            kCGEventFlagMaskAlternate,
            kCGEventFlagMaskCommand,
        };
        for (CGEventFlags bit : kWatchedBits) {
            bool was = (impl->last_flags & bit) != 0;
            bool now = (flags         & bit) != 0;
            if (was == now) continue;
            std::string name = macos_flag_bit_to_modifier(bit);
            if (name.empty()) continue;
            if (now) impl->manager->on_modifier_press(name);
            else     impl->manager->on_modifier_release(name);
        }
        impl->last_flags = flags;
    } else if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        CGKeyCode vk = static_cast<CGKeyCode>(
            CGEventGetIntegerValueField(ev, kCGKeyboardEventKeycode));
        std::string name = macos_keycode_to_name(vk);
        if (!name.empty()) {
            if (type == kCGEventKeyDown) impl->manager->on_key_press(name);
            else                         impl->manager->on_key_release(name);
        }
    }
    return ev;
}

HotkeyManager::HotkeyManager(const HotkeyConfig& config, EventCallback callback)
    : config_(config), callback_(std::move(callback)),
      impl_(std::make_unique<Impl>()) {
    impl_->manager = this;
    for (const auto& t : config.trigger) trigger_combos_.push_back(KeyCombo::parse(t));
    for (const auto& c : config.cancel)  cancel_combos_.push_back(KeyCombo::parse(c));
}

HotkeyManager::~HotkeyManager() { stop(); }

void HotkeyManager::start() {
    if (running_.load()) {
        spdlog::warn("Hotkey listener already running");
        return;
    }

    if (!CGPreflightListenEventAccess()) {
        spdlog::error("Input Monitoring permission denied. Hotkeys disabled.");
        spdlog::error("  Fix: System Settings \u2192 Privacy & Security \u2192 "
                      "Input Monitoring \u2192 add AutoWhisper");
        // Best-effort: trigger the system prompt so the user sees it.
        CGRequestListenEventAccess();
        return;
    }

    running_.store(true);

    // If stop() races ahead of the listener thread's CFRunLoopGetCurrent()
    // call, CFRunLoopStop would be a no-op and CFRunLoopRun would block
    // forever. The listener thread re-checks running_ AFTER publishing
    // run_loop but BEFORE entering the loop — so a fast stop() caught
    // between those two points still exits cleanly.
    impl_->listener = std::thread([this]() {
        const CGEventMask mask =
            CGEventMaskBit(kCGEventKeyDown) |
            CGEventMaskBit(kCGEventKeyUp)   |
            CGEventMaskBit(kCGEventFlagsChanged);

        impl_->tap = CGEventTapCreate(
            kCGSessionEventTap,
            kCGHeadInsertEventTap,
            kCGEventTapOptionListenOnly,   // listen-only: no latency impact
            mask,
            &Impl::tap_cb,
            impl_.get());

        if (!impl_->tap) {
            spdlog::error("CGEventTapCreate returned NULL");
            running_.store(false);
            return;
        }

        impl_->src = CFMachPortCreateRunLoopSource(
            kCFAllocatorDefault, impl_->tap, 0);
        if (!impl_->src) {
            spdlog::error("CFMachPortCreateRunLoopSource failed");
            CFRelease(impl_->tap);
            impl_->tap = nullptr;
            running_.store(false);
            return;
        }

        impl_->run_loop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(impl_->run_loop, impl_->src, kCFRunLoopCommonModes);
        CGEventTapEnable(impl_->tap, true);

        // Race guard: if stop() fired after running_=true but before
        // run_loop was published, its CFRunLoopStop was a no-op. Check
        // running_ here before entering CFRunLoopRun — if false, skip.
        if (!running_.load()) {
            spdlog::info("CGEventTap: early stop signaled before run loop");
            CFRunLoopRemoveSource(impl_->run_loop, impl_->src, kCFRunLoopCommonModes);
            CFRelease(impl_->src);
            impl_->src = nullptr;
            CGEventTapEnable(impl_->tap, false);
            CFRelease(impl_->tap);
            impl_->tap = nullptr;
            impl_->run_loop = nullptr;
            return;
        }

        spdlog::info("CGEventTap listening (listen-only)");
        CFRunLoopRun();
        spdlog::info("CGEventTap run loop exited");

        // The listener thread releases the event tap because the run-loop source
        // belongs to this thread's CFRunLoop. Removing it later from the main
        // thread can trip CoreFoundation pointer-auth checks on modern macOS.
        if (impl_->src) {
            CFRunLoopRemoveSource(impl_->run_loop, impl_->src, kCFRunLoopCommonModes);
            CFRelease(impl_->src);
            impl_->src = nullptr;
        }
        if (impl_->tap) {
            CGEventTapEnable(impl_->tap, false);
            CFRelease(impl_->tap);
            impl_->tap = nullptr;
        }
        impl_->run_loop = nullptr;
    });
}

void HotkeyManager::signal_stop() {
    if (!running_.load()) return;
    spdlog::info("Signaling hotkey listener to stop");
    running_.store(false);
    if (impl_->run_loop) CFRunLoopStop(impl_->run_loop);
}

void HotkeyManager::stop() {
    running_.store(false);
    if (impl_->run_loop) CFRunLoopStop(impl_->run_loop);
    if (impl_->listener.joinable()) impl_->listener.join();

    // The listener thread releases the event tap/run-loop source because they
    // belong to its CFRunLoop. Only clear logical state here after join.
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

} // namespace autowhisper

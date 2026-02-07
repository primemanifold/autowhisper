#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>

#include <cstring>
#include <thread>

namespace autowhisper {

struct HotkeyManager::Impl {
    HotkeyManager* manager = nullptr;
    Display* data_display = nullptr;
    Display* ctrl_display = nullptr;
    XRecordContext record_ctx = 0;
    std::thread listener_thread;

    static std::string keycode_to_modifier(Display* dpy, unsigned int keycode) {
        KeySym ks = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
        switch (ks) {
            case XK_Shift_L: case XK_Shift_R: return "shift";
            case XK_Control_L: case XK_Control_R: return "ctrl";
            case XK_Alt_L: case XK_Alt_R: return "alt";
            case XK_Super_L: case XK_Super_R:
            case XK_Meta_L: case XK_Meta_R: return "super";
            default: return "";
        }
    }

    static std::string keycode_to_name(Display* dpy, unsigned int keycode) {
        KeySym ks = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
        if (ks == NoSymbol) return "";

        const char* name = XKeysymToString(ks);
        if (!name) return "";

        std::string result(name);
        // Normalize some names
        if (result == "Escape") return "esc";
        if (result == "Return") return "return";
        if (result == "space") return "space";

        // Lowercase single chars
        if (result.size() == 1) {
            result[0] = static_cast<char>(tolower(result[0]));
        }

        // Lowercase function keys
        if (result.size() > 1 && result[0] == 'F' && isdigit(result[1])) {
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        }

        return result;
    }

    static void record_callback(XPointer closure, XRecordInterceptData* hook) {
        auto* impl = reinterpret_cast<Impl*>(closure);
        if (!impl || !impl->manager || !impl->manager->running_.load()) {
            XRecordFreeData(hook);
            return;
        }

        if (hook->category != XRecordFromServer) {
            XRecordFreeData(hook);
            return;
        }

        unsigned int keycode = hook->data[1];
        int type = hook->data[0];

        if (type == KeyPress) {
            std::string mod = keycode_to_modifier(impl->data_display, keycode);
            if (!mod.empty()) {
                impl->manager->on_modifier_press(mod);
            } else {
                std::string name = keycode_to_name(impl->data_display, keycode);
                if (!name.empty()) {
                    impl->manager->on_key_press(name);
                }
            }
        } else if (type == KeyRelease) {
            std::string mod = keycode_to_modifier(impl->data_display, keycode);
            if (!mod.empty()) {
                impl->manager->on_modifier_release(mod);
            } else {
                std::string name = keycode_to_name(impl->data_display, keycode);
                if (!name.empty()) {
                    impl->manager->on_key_release(name);
                }
            }
        }

        XRecordFreeData(hook);
    }
};

HotkeyManager::HotkeyManager(const HotkeyConfig& config, EventCallback callback)
    : config_(config),
      callback_(std::move(callback)),
      impl_(std::make_unique<Impl>()) {
    impl_->manager = this;

    // Parse combos
    for (const auto& t : config.trigger) {
        trigger_combos_.push_back(KeyCombo::parse(t));
    }
    for (const auto& c : config.cancel) {
        cancel_combos_.push_back(KeyCombo::parse(c));
    }
}

HotkeyManager::~HotkeyManager() {
    stop();
}

void HotkeyManager::start() {
    if (running_.load()) {
        spdlog::warn("Hotkey listener already running");
        return;
    }

    spdlog::info("Starting hotkey listener: trigger=[{}], cancel=[{}], mode={}",
                 [&]{
                     std::string s;
                     for (const auto& t : config_.trigger) { if (!s.empty()) s += ", "; s += t; }
                     return s;
                 }(),
                 [&]{
                     std::string s;
                     for (const auto& c : config_.cancel) { if (!s.empty()) s += ", "; s += c; }
                     return s;
                 }(),
                 config_.mode);

    running_.store(true);

    impl_->listener_thread = std::thread([this]() {
        // Open two displays: one for data, one for control
        impl_->data_display = XOpenDisplay(nullptr);
        impl_->ctrl_display = XOpenDisplay(nullptr);

        if (!impl_->data_display || !impl_->ctrl_display) {
            spdlog::error("Failed to open X11 display for hotkey listener");
            running_.store(false);
            return;
        }

        // Check XRecord extension
        int major, minor;
        if (!XRecordQueryVersion(impl_->ctrl_display, &major, &minor)) {
            spdlog::error("XRecord extension not available");
            XCloseDisplay(impl_->data_display);
            XCloseDisplay(impl_->ctrl_display);
            impl_->data_display = nullptr;
            impl_->ctrl_display = nullptr;
            running_.store(false);
            return;
        }

        // Set up recording
        XRecordRange* range = XRecordAllocRange();
        if (!range) {
            spdlog::error("Failed to allocate XRecord range");
            XCloseDisplay(impl_->data_display);
            XCloseDisplay(impl_->ctrl_display);
            impl_->data_display = nullptr;
            impl_->ctrl_display = nullptr;
            running_.store(false);
            return;
        }

        range->device_events.first = KeyPress;
        range->device_events.last = KeyRelease;

        XRecordClientSpec clients = XRecordAllClients;
        impl_->record_ctx = XRecordCreateContext(impl_->ctrl_display, 0, &clients, 1, &range, 1);
        XFree(range);

        if (!impl_->record_ctx) {
            spdlog::error("Failed to create XRecord context");
            XCloseDisplay(impl_->data_display);
            XCloseDisplay(impl_->ctrl_display);
            impl_->data_display = nullptr;
            impl_->ctrl_display = nullptr;
            running_.store(false);
            return;
        }

        XSync(impl_->ctrl_display, False);

        // This blocks until XRecordDisableContext is called
        XRecordEnableContext(impl_->data_display, impl_->record_ctx,
                             Impl::record_callback, reinterpret_cast<XPointer>(impl_.get()));
    });
}

void HotkeyManager::stop() {
    if (!running_.load()) return;

    spdlog::info("Stopping hotkey listener");
    running_.store(false);

    // Disable the record context to unblock XRecordEnableContext
    if (impl_->ctrl_display && impl_->record_ctx) {
        XRecordDisableContext(impl_->ctrl_display, impl_->record_ctx);
        XRecordFreeContext(impl_->ctrl_display, impl_->record_ctx);
        impl_->record_ctx = 0;
    }

    if (impl_->listener_thread.joinable()) {
        impl_->listener_thread.join();
    }

    if (impl_->data_display) {
        XCloseDisplay(impl_->data_display);
        impl_->data_display = nullptr;
    }
    if (impl_->ctrl_display) {
        XCloseDisplay(impl_->ctrl_display);
        impl_->ctrl_display = nullptr;
    }

    // Reset state
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

} // namespace autowhisper

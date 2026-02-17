#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>

#include <cstring>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

namespace autowhisper {

static struct XThreadInit {
    XThreadInit() { XInitThreads(); }
} s_x_thread_init;

struct HotkeyManager::Impl {
    HotkeyManager* manager = nullptr;
    Display* data_display = nullptr;
    Display* ctrl_display = nullptr;
    XRecordContext record_ctx = 0;
    std::thread listener_thread;
    int wake_pipe[2] = {-1, -1};  // pipe to signal thread to stop

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
        if (result == "Escape") return "esc";
        if (result == "Return") return "return";
        if (result == "space") return "space";

        if (result.size() == 1) {
            result[0] = static_cast<char>(tolower(result[0]));
        }

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
            std::string mod = keycode_to_modifier(impl->ctrl_display, keycode);
            if (!mod.empty()) {
                impl->manager->on_modifier_press(mod);
            } else {
                std::string name = keycode_to_name(impl->ctrl_display, keycode);
                if (!name.empty()) {
                    impl->manager->on_key_press(name);
                }
            }
        } else if (type == KeyRelease) {
            std::string mod = keycode_to_modifier(impl->ctrl_display, keycode);
            if (!mod.empty()) {
                impl->manager->on_modifier_release(mod);
            } else {
                std::string name = keycode_to_name(impl->ctrl_display, keycode);
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

    // Create wake pipe for signaling the thread to stop
    if (pipe(impl_->wake_pipe) != 0) {
        spdlog::error("Failed to create wake pipe");
        return;
    }

    running_.store(true);

    impl_->listener_thread = std::thread([this]() {
        impl_->ctrl_display = XOpenDisplay(nullptr);
        impl_->data_display = XOpenDisplay(nullptr);

        if (!impl_->data_display || !impl_->ctrl_display) {
            spdlog::error("Failed to open X11 display for hotkey listener");
            running_.store(false);
            return;
        }

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

        // Use async API so we never block indefinitely
        if (!XRecordEnableContextAsync(impl_->data_display, impl_->record_ctx,
                                        Impl::record_callback,
                                        reinterpret_cast<XPointer>(impl_.get()))) {
            spdlog::error("Failed to enable XRecord context");
            running_.store(false);
            return;
        }

        // Flush the enable request to the X server — without this,
        // the request sits in Xlib's output buffer and the server
        // never starts delivering XRecord events.
        XFlush(impl_->data_display);

        int x11_fd = ConnectionNumber(impl_->data_display);
        int pipe_fd = impl_->wake_pipe[0];
        int max_fd = std::max(x11_fd, pipe_fd) + 1;

        while (running_.load()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(x11_fd, &fds);
            FD_SET(pipe_fd, &fds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 200000;  // 200ms

            int ret = select(max_fd, &fds, nullptr, nullptr, &tv);

            if (!running_.load()) break;

            if (ret > 0 && FD_ISSET(pipe_fd, &fds)) {
                spdlog::debug("Hotkey thread: pipe wakeup received");
                break;
            }

            if (ret > 0 && FD_ISSET(x11_fd, &fds)) {
                XRecordProcessReplies(impl_->data_display);
            }
        }

        // Clean up XRecord
        spdlog::debug("Hotkey thread: disabling XRecord context");
        XRecordDisableContext(impl_->ctrl_display, impl_->record_ctx);
        spdlog::debug("Hotkey thread: freeing XRecord context");
        XRecordFreeContext(impl_->ctrl_display, impl_->record_ctx);
        impl_->record_ctx = 0;

        spdlog::debug("Hotkey thread: closing displays");
        XCloseDisplay(impl_->data_display);
        XCloseDisplay(impl_->ctrl_display);
        impl_->data_display = nullptr;
        impl_->ctrl_display = nullptr;
        spdlog::debug("Hotkey thread: done");
    });
}

void HotkeyManager::stop() {
    if (!running_.load()) return;

    spdlog::info("Stopping hotkey listener");
    running_.store(false);

    // Write to pipe to wake the select() immediately
    if (impl_->wake_pipe[1] >= 0) {
        char c = 1;
        ssize_t n = write(impl_->wake_pipe[1], &c, 1);
        (void)n;
    }

    if (impl_->listener_thread.joinable()) {
        impl_->listener_thread.join();
    }

    // Close pipe
    if (impl_->wake_pipe[0] >= 0) { close(impl_->wake_pipe[0]); impl_->wake_pipe[0] = -1; }
    if (impl_->wake_pipe[1] >= 0) { close(impl_->wake_pipe[1]); impl_->wake_pipe[1] = -1; }

    // Reset state
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

} // namespace autowhisper

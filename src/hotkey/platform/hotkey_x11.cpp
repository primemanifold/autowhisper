#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>

#include <chrono>
#include <cstring>
#include <mutex>
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

    // Guards ctrl_display/record_ctx lifetime so stop()/signal_stop() can
    // nudge a listener stuck inside XRecordProcessReplies() without racing
    // the thread's own cleanup. timed_mutex: if the thread is wedged in
    // XCloseDisplay while holding it, the nudge is skipped rather than
    // turning stop() into a second hang.
    std::timed_mutex x_mutex;
    std::atomic<bool> thread_exited{false};

    // Asks the X server to end the record stream. This is the documented
    // way to unblock XRecordProcessReplies: the data display receives an
    // end-of-data reply and returns. Safe cross-thread after XInitThreads.
    void nudge_record_shutdown() {
        std::unique_lock<std::timed_mutex> lock(x_mutex, std::defer_lock);
        if (!lock.try_lock_for(std::chrono::seconds(1))) return;
        if (ctrl_display && record_ctx) {
            XRecordDisableContext(ctrl_display, record_ctx);
            XFlush(ctrl_display);
        }
    }

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
    impl_->thread_exited.store(false);

    // The thread works through a raw Impl pointer captured here: if stop()
    // ever has to abandon the thread it leaks this Impl and re-creates
    // impl_, so the zombie must never dereference the member again.
    impl_->listener_thread = std::thread([this, impl = impl_.get()]() {
        struct ExitFlag {
            std::atomic<bool>& flag;
            ~ExitFlag() { flag.store(true); }
        } exit_flag{impl->thread_exited};

        {
            std::lock_guard<std::timed_mutex> lock(impl->x_mutex);
            impl->ctrl_display = XOpenDisplay(nullptr);
            impl->data_display = XOpenDisplay(nullptr);
        }

        auto fail_cleanup = [this, impl]() {
            std::lock_guard<std::timed_mutex> lock(impl->x_mutex);
            if (impl->data_display) {
                XCloseDisplay(impl->data_display);
                impl->data_display = nullptr;
            }
            if (impl->ctrl_display) {
                XCloseDisplay(impl->ctrl_display);
                impl->ctrl_display = nullptr;
            }
            running_.store(false);
        };

        if (!impl->data_display || !impl->ctrl_display) {
            spdlog::error("Failed to open X11 display for hotkey listener");
            fail_cleanup();
            return;
        }

        int major, minor;
        if (!XRecordQueryVersion(impl->ctrl_display, &major, &minor)) {
            spdlog::error("XRecord extension not available");
            fail_cleanup();
            return;
        }

        XRecordRange* range = XRecordAllocRange();
        if (!range) {
            spdlog::error("Failed to allocate XRecord range");
            fail_cleanup();
            return;
        }

        range->device_events.first = KeyPress;
        range->device_events.last = KeyRelease;

        XRecordClientSpec clients = XRecordAllClients;
        {
            std::lock_guard<std::timed_mutex> lock(impl->x_mutex);
            impl->record_ctx =
                XRecordCreateContext(impl->ctrl_display, 0, &clients, 1, &range, 1);
        }
        XFree(range);

        if (!impl->record_ctx) {
            spdlog::error("Failed to create XRecord context");
            fail_cleanup();
            return;
        }

        XSync(impl->ctrl_display, False);

        // Use async API so we never block indefinitely
        if (!XRecordEnableContextAsync(impl->data_display, impl->record_ctx,
                                        Impl::record_callback,
                                        reinterpret_cast<XPointer>(impl))) {
            spdlog::error("Failed to enable XRecord context");
            running_.store(false);
            return;
        }

        // Flush the enable request to the X server — without this,
        // the request sits in Xlib's output buffer and the server
        // never starts delivering XRecord events.
        XFlush(impl->data_display);

        int x11_fd = ConnectionNumber(impl->data_display);
        int pipe_fd = impl->wake_pipe[0];
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
                XRecordProcessReplies(impl->data_display);
            }
        }

        // Clean up XRecord.  With the async API the data_display
        // connection may be waiting for server replies, so
        // XRecordDisableContext can block.  Closing the displays is
        // sufficient — the X server tears down the record context
        // automatically when the connection drops.
        {
            std::lock_guard<std::timed_mutex> lock(impl->x_mutex);
            XCloseDisplay(impl->data_display);
            impl->data_display = nullptr;

            if (impl->record_ctx) {
                XRecordFreeContext(impl->ctrl_display, impl->record_ctx);
                impl->record_ctx = 0;
            }

            XCloseDisplay(impl->ctrl_display);
            impl->ctrl_display = nullptr;
        }
    });
}

void HotkeyManager::signal_stop() {
    if (!running_.load()) return;

    spdlog::info("Signaling hotkey listener to stop");
    running_.store(false);

    // Write to pipe to wake the select() immediately
    if (impl_->wake_pipe[1] >= 0) {
        char c = 1;
        ssize_t n = write(impl_->wake_pipe[1], &c, 1);
        (void)n;
    }

    // End the record stream so a thread inside XRecordProcessReplies()
    // returns instead of waiting for more data.
    impl_->nudge_record_shutdown();
}

void HotkeyManager::stop() {
    running_.store(false);

    if (!impl_->listener_thread.joinable()) {
        // Nothing to wait for; just release pipe fds and cached state.
        if (impl_->wake_pipe[0] >= 0) { close(impl_->wake_pipe[0]); impl_->wake_pipe[0] = -1; }
        if (impl_->wake_pipe[1] >= 0) { close(impl_->wake_pipe[1]); impl_->wake_pipe[1] = -1; }
        pressed_modifiers_.clear();
        trigger_pressed_ = false;
        active_trigger_.reset();
        return;
    }

    // Always write to wake pipe — signal_stop() may have been called
    // earlier without the thread having drained the pipe yet, or the
    // thread may be blocked in select() and needs another nudge.
    if (impl_->wake_pipe[1] >= 0) {
        char c = 1;
        ssize_t n = write(impl_->wake_pipe[1], &c, 1);
        (void)n;
    }

    impl_->nudge_record_shutdown();

    // Bounded wait. The historical settings pause/resume deadlock hung
    // forever in join() while the listener sat inside
    // XRecordProcessReplies(); never block shutdown on the X server.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!impl_->thread_exited.load() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (impl_->thread_exited.load()) {
        impl_->listener_thread.join();
    } else {
        // Last resort: the X server is unresponsive and the listener cannot
        // exit. Detach and leak the Impl so the zombie thread never touches
        // freed memory; a bounded leak beats a deadlocked or aborted app.
        spdlog::error(
            "Hotkey listener did not exit within 5s (X server unresponsive?); "
            "detaching listener thread and leaking its resources");
        impl_->listener_thread.detach();
        (void)impl_.release();
        impl_ = std::make_unique<Impl>();
        impl_->manager = this;
    }

    // Close pipe
    if (impl_->wake_pipe[0] >= 0) { close(impl_->wake_pipe[0]); impl_->wake_pipe[0] = -1; }
    if (impl_->wake_pipe[1] >= 0) { close(impl_->wake_pipe[1]); impl_->wake_pipe[1] = -1; }

    // Safe after listener thread has exited (or been abandoned).
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

} // namespace autowhisper

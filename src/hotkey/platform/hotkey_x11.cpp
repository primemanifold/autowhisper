#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>

#include <pthread.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

namespace autowhisper {

namespace {

// Marks the XRecord listener thread so the IO error handler can tell it
// apart from the rest of the process.
thread_local bool t_is_listener_thread = false;

XIOErrorHandler g_previous_io_handler = nullptr;

// Xlib's default IO error handler exits the whole process. If the listener
// thread's connection dies (X server gone, or a connection abandoned by the
// bounded-stop fallback), only that thread should die — dictation degrades
// instead of taking the daemon down. pthread_exit unwinds the thread's
// stack, so the exit flag destructor still fires.
int listener_io_error_handler(Display* dpy) {
    if (t_is_listener_thread) {
        spdlog::error("X11 hotkey listener lost its X connection; stopping listener thread");
        pthread_exit(nullptr);
    }
    if (g_previous_io_handler && g_previous_io_handler != listener_io_error_handler) {
        return g_previous_io_handler(dpy);
    }
    spdlog::critical("Fatal X11 IO error");
    _exit(1);
}

struct XThreadInit {
    XThreadInit() { XInitThreads(); }
};
XThreadInit s_x_thread_init;

}  // namespace

struct HotkeyManager::Impl {
    HotkeyManager* manager = nullptr;
    Display* data_display = nullptr;
    Display* ctrl_display = nullptr;
    XRecordContext record_ctx = 0;
    std::thread listener_thread;

    // Guards ctrl_display/record_ctx lifetime so stop() can issue
    // XRecordDisableContext without racing the thread's own cleanup.
    // timed_mutex: if the thread is wedged in an X call while holding it,
    // the nudge is skipped rather than turning stop() into a second hang.
    std::timed_mutex x_mutex;
    std::atomic<bool> thread_exited{false};

    // Asks the X server to end the record stream. The listener blocks inside
    // the synchronous XRecordEnableContext; this is the documented way to
    // make it return. Issued from the control connection, which is safe
    // cross-thread after XInitThreads.
    void nudge_record_shutdown() {
        std::unique_lock<std::timed_mutex> lock(x_mutex, std::defer_lock);
        if (!lock.try_lock_for(std::chrono::milliseconds(500))) return;
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
    for (const auto& t : config.ask_trigger) {
        ask_trigger_combos_.push_back(KeyCombo::parse(t));
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

    // Install after GTK (or anything else) has set its own handler so we
    // can chain to it for non-listener threads.
    XIOErrorHandler previous = XSetIOErrorHandler(listener_io_error_handler);
    if (previous != listener_io_error_handler) {
        g_previous_io_handler = previous;
    }

    running_.store(true);
    impl_->thread_exited.store(false);

    // The thread works through a raw Impl pointer captured here: if stop()
    // ever has to abandon the thread it leaks this Impl and re-creates
    // impl_, so the zombie must never dereference the member again.
    impl_->listener_thread = std::thread([this, impl = impl_.get()]() {
        t_is_listener_thread = true;

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

        // The create request must reach the server before the data
        // connection refers to the context.
        XSync(impl->ctrl_display, False);

        // Canonical XRecord pattern: the synchronous enable blocks here,
        // dispatching record_callback for each event, and returns when
        // XRecordDisableContext is issued on the control connection
        // (stop()'s nudge loop). No bespoke select/pipe machinery.
        if (!XRecordEnableContext(impl->data_display, impl->record_ctx,
                                  Impl::record_callback,
                                  reinterpret_cast<XPointer>(impl))) {
            spdlog::error("Failed to enable XRecord context");
        }

        std::lock_guard<std::timed_mutex> lock(impl->x_mutex);
        XCloseDisplay(impl->data_display);
        impl->data_display = nullptr;

        if (impl->record_ctx) {
            XRecordFreeContext(impl->ctrl_display, impl->record_ctx);
            impl->record_ctx = 0;
        }

        XCloseDisplay(impl->ctrl_display);
        impl->ctrl_display = nullptr;
    });
}

void HotkeyManager::signal_stop() {
    if (!running_.load()) return;

    spdlog::info("Signaling hotkey listener to stop");
    running_.store(false);
    impl_->nudge_record_shutdown();
}

void HotkeyManager::stop() {
    running_.store(false);

    if (!impl_->listener_thread.joinable()) {
        pressed_modifiers_.clear();
        trigger_pressed_ = false;
        active_trigger_.reset();
        return;
    }

    // Repeatedly ask the server to end the record stream until the thread
    // exits. Retrying closes the startup race where a single nudge lands
    // before the context is enabled and would otherwise be lost — the
    // historical settings resume path deadlocked exactly like that
    // (docs/hotkey-resume-deadlock.md). Never block shutdown on the X
    // server: bound the whole wait.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!impl_->thread_exited.load() &&
           std::chrono::steady_clock::now() < deadline) {
        impl_->nudge_record_shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (impl_->thread_exited.load()) {
        impl_->listener_thread.join();
    } else {
        // Last resort: the X server is unresponsive and the listener cannot
        // exit. Detach and leak the Impl so the zombie thread never touches
        // freed memory; a bounded leak beats a deadlocked or aborted app.
        // If the zombie's connection later errors, the IO error handler
        // exits only that thread.
        spdlog::error(
            "Hotkey listener did not exit within 5s (X server unresponsive?); "
            "detaching listener thread and leaking its resources");
        impl_->listener_thread.detach();
        Impl* leaked = impl_.release();
        leaked->manager = nullptr;
        impl_ = std::make_unique<Impl>();
        impl_->manager = this;
    }

    // Safe after listener thread has exited (or been abandoned).
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

} // namespace autowhisper

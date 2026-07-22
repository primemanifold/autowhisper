// Integration tests for the X11 hotkey listener, run against a private Xvfb
// server with keys synthesized through XTest. Skipped when Xvfb is missing.
//
// The stop() timing assertions are the regression net for the historical
// settings pause/resume deadlock (docs/hotkey-resume-deadlock.md): stop()
// must never block indefinitely on the listener thread.

#include <catch2/catch_test_macros.hpp>

#include "hotkey/hotkey.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace autowhisper;
using namespace std::chrono_literals;

namespace {

bool xvfb_available() {
    return std::filesystem::exists("/usr/bin/Xvfb") ||
           std::filesystem::exists("/usr/local/bin/Xvfb");
}

// Spawns a private Xvfb and points $DISPLAY at it. Tries a few fixed
// display numbers and polls XOpenDisplay until the server accepts
// connections (the xvfb-run -a strategy; -displayfd proved flaky under
// ctest's fd environment in CI).
struct XvfbServer {
    pid_t pid = -1;
    std::string display;
    std::string saved_display;
    bool had_display = false;

    bool launch() {
        const char* prev = std::getenv("DISPLAY");
        had_display = prev != nullptr;
        if (prev) saved_display = prev;

        for (int number : {99, 98, 97, 96}) {
            display = ":" + std::to_string(number);

            pid = fork();
            if (pid < 0) return false;
            if (pid == 0) {
                execlp("Xvfb", "Xvfb", display.c_str(), "-screen", "0",
                       "640x480x24", "-nolisten", "tcp", (char*)nullptr);
                _exit(127);
            }

            // Poll until the server accepts connections or the child dies
            // (e.g. display already locked by another Xvfb).
            const auto deadline = std::chrono::steady_clock::now() + 10s;
            while (std::chrono::steady_clock::now() < deadline) {
                int status = 0;
                if (waitpid(pid, &status, WNOHANG) == pid) {
                    pid = -1;  // child exited; try the next display number
                    break;
                }
                if (Display* probe = XOpenDisplay(display.c_str())) {
                    XCloseDisplay(probe);
                    setenv("DISPLAY", display.c_str(), 1);
                    return true;
                }
                std::this_thread::sleep_for(100ms);
            }
            if (pid > 0) {
                // Server never became ready; clean up before trying the next.
                kill(pid, SIGTERM);
                int status = 0;
                waitpid(pid, &status, 0);
                pid = -1;
            }
        }
        return false;
    }

    void terminate() {
        if (pid > 0) {
            kill(pid, SIGTERM);
            int status = 0;
            waitpid(pid, &status, 0);
            pid = -1;
        }
        if (had_display) {
            setenv("DISPLAY", saved_display.c_str(), 1);
        } else {
            unsetenv("DISPLAY");
        }
    }

    ~XvfbServer() { terminate(); }
};

struct EventSink {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<HotkeyEvent> events;

    void push(HotkeyEvent e) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(e);
        cv.notify_all();
    }

    bool wait_for_count(size_t n, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [&] { return events.size() >= n; });
    }

    bool wait_for_event(HotkeyEvent expected, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [&] {
            return std::find(events.begin(), events.end(), expected) != events.end();
        });
    }

    std::vector<HotkeyEvent> snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        return events;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        events.clear();
    }
};

void fake_key(Display* dpy, KeySym sym, bool press) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    REQUIRE(code != 0);
    XTestFakeKeyEvent(dpy, code, press ? True : False, CurrentTime);
    XSync(dpy, False);
}

HotkeyConfig push_to_talk_config() {
    HotkeyConfig cfg;
    cfg.mode = "push_to_talk";
    cfg.trigger = {"ctrl+shift"};
    cfg.cancel = {};
    cfg.escape_to_cancel = false;
    return cfg;
}

}  // namespace

TEST_CASE("X11 push-to-talk delivers START/STOP and stop() is bounded",
          "[hotkey_x11][integration]") {
    if (!xvfb_available()) SKIP("Xvfb not installed");

    XvfbServer xvfb;
    REQUIRE(xvfb.launch());

    EventSink sink;
    HotkeyManager mgr(push_to_talk_config(), [&](HotkeyEvent e) { sink.push(e); });
    mgr.start();

    Display* client = nullptr;
    for (int i = 0; i < 50 && !client; i++) {
        client = XOpenDisplay(nullptr);
        if (!client) std::this_thread::sleep_for(100ms);
    }
    REQUIRE(client != nullptr);

    int xtest_event_base, xtest_error_base, maj, min;
    REQUIRE(XTestQueryExtension(client, &xtest_event_base, &xtest_error_base, &maj, &min));

    // The listener thread needs a moment to register its XRecord context.
    // Re-send the chord until the START event lands.
    bool started = false;
    for (int attempt = 0; attempt < 10 && !started; attempt++) {
        std::this_thread::sleep_for(200ms);
        fake_key(client, XK_Control_L, true);
        fake_key(client, XK_Shift_L, true);
        started = sink.wait_for_count(1, 500ms);
        if (!started) {
            fake_key(client, XK_Shift_L, false);
            fake_key(client, XK_Control_L, false);
        }
    }
    REQUIRE(started);
    CHECK(sink.snapshot().front() == HotkeyEvent::START);

    fake_key(client, XK_Shift_L, false);
    REQUIRE(sink.wait_for_count(2, 2000ms));
    CHECK(sink.snapshot()[1] == HotkeyEvent::STOP);

    fake_key(client, XK_Control_L, false);
    std::this_thread::sleep_for(200ms);
    sink.clear();

    // The opt-in Ask Fabric binding is a distinct event pair. The daemon can
    // latch this mode before recording and cannot silently fall back to
    // Dictate based on transcript content.
    fake_key(client, XK_Control_L, true);
    fake_key(client, XK_Alt_L, true);
    fake_key(client, XK_space, true);
    REQUIRE(sink.wait_for_event(HotkeyEvent::ASK_START, 2000ms));
    fake_key(client, XK_space, false);
    REQUIRE(sink.wait_for_event(HotkeyEvent::ASK_STOP, 2000ms));
    fake_key(client, XK_Alt_L, false);
    fake_key(client, XK_Control_L, false);

    XCloseDisplay(client);

    const auto t0 = std::chrono::steady_clock::now();
    mgr.stop();
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    CHECK(elapsed < 5s);
}

TEST_CASE("X11 set_config swaps the live trigger without a restart",
          "[hotkey_x11][integration]") {
    if (!xvfb_available()) SKIP("Xvfb not installed");

    XvfbServer xvfb;
    REQUIRE(xvfb.launch());

    EventSink sink;
    // Start on ctrl+shift, then live-swap to ctrl+alt — the daemon's
    // config-reload path. The new chord must fire; the old must not.
    HotkeyManager mgr(push_to_talk_config(), [&](HotkeyEvent e) { sink.push(e); });
    mgr.start();

    Display* client = nullptr;
    for (int i = 0; i < 50 && !client; i++) {
        client = XOpenDisplay(nullptr);
        if (!client) std::this_thread::sleep_for(100ms);
    }
    REQUIRE(client != nullptr);
    int b1, b2, mj, mn;
    REQUIRE(XTestQueryExtension(client, &b1, &b2, &mj, &mn));

    HotkeyConfig updated;
    updated.mode = "push_to_talk";
    updated.trigger = {"ctrl+alt"};
    updated.cancel = {};
    updated.escape_to_cancel = false;
    mgr.set_config(updated);

    // The new chord (ctrl+alt) fires START, then STOP on release.
    bool started = false;
    for (int attempt = 0; attempt < 10 && !started; attempt++) {
        std::this_thread::sleep_for(200ms);
        fake_key(client, XK_Control_L, true);
        fake_key(client, XK_Alt_L, true);
        started = sink.wait_for_count(1, 500ms);
        if (!started) {
            fake_key(client, XK_Alt_L, false);
            fake_key(client, XK_Control_L, false);
        }
    }
    REQUIRE(started);
    CHECK(sink.snapshot().front() == HotkeyEvent::START);
    fake_key(client, XK_Alt_L, false);
    REQUIRE(sink.wait_for_count(2, 2000ms));
    CHECK(sink.snapshot()[1] == HotkeyEvent::STOP);

    // The old chord (ctrl+shift) must no longer do anything.
    const size_t count_before = sink.snapshot().size();
    fake_key(client, XK_Control_L, true);
    fake_key(client, XK_Shift_L, true);
    std::this_thread::sleep_for(400ms);
    fake_key(client, XK_Shift_L, false);
    fake_key(client, XK_Control_L, false);
    CHECK(sink.snapshot().size() == count_before);

    XCloseDisplay(client);
    mgr.stop();
}

TEST_CASE("X11 listener start/stop cycles never hang (resume-deadlock regression)",
          "[hotkey_x11][integration]") {
    if (!xvfb_available()) SKIP("Xvfb not installed");

    XvfbServer xvfb;
    REQUIRE(xvfb.launch());

    Display* client = nullptr;
    for (int i = 0; i < 50 && !client; i++) {
        client = XOpenDisplay(nullptr);
        if (!client) std::this_thread::sleep_for(100ms);
    }
    REQUIRE(client != nullptr);

    // The historical deadlock shape: stop an active listener (as the old
    // settings resume path did) while key traffic flows, then start again.
    for (int cycle = 0; cycle < 3; cycle++) {
        EventSink sink;
        HotkeyManager mgr(push_to_talk_config(), [&](HotkeyEvent e) { sink.push(e); });
        mgr.start();
        std::this_thread::sleep_for(300ms);

        // Generate record traffic so the listener is actively processing.
        for (int i = 0; i < 5; i++) {
            fake_key(client, XK_Control_L, true);
            fake_key(client, XK_Control_L, false);
        }

        const auto t0 = std::chrono::steady_clock::now();
        mgr.stop();
        const auto elapsed = std::chrono::steady_clock::now() - t0;
        INFO("cycle " << cycle);
        CHECK(elapsed < 5s);
    }

    XCloseDisplay(client);
}

TEST_CASE("X11 stop() without start() is a no-op", "[hotkey_x11]") {
    EventSink sink;
    HotkeyManager mgr(push_to_talk_config(), [&](HotkeyEvent e) { sink.push(e); });
    const auto t0 = std::chrono::steady_clock::now();
    mgr.stop();
    CHECK(std::chrono::steady_clock::now() - t0 < 1s);
    CHECK(sink.snapshot().empty());
}

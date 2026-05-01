#include <spdlog/spdlog.h>

#import <AppKit/AppKit.h>

#include <atomic>
#include <csignal>
#include <dispatch/dispatch.h>
#include <functional>
#include <thread>

namespace autowhisper {

namespace {

dispatch_source_t g_sigint_src  = nullptr;
dispatch_source_t g_sigterm_src = nullptr;
std::atomic<bool>* g_shutdown_flag = nullptr;

} // namespace

// Install dispatch sources for SIGINT/SIGTERM. These fire on a dispatch
// queue (not in an async-signal-unsafe context), so we can safely call
// [NSApp stop:] inside the handler.
void aw_macos_setup_signals(std::atomic<bool>* shutdown_flag) {
    g_shutdown_flag = shutdown_flag;

    // Ignore default signal disposition so dispatch can receive them.
    signal(SIGINT,  SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    dispatch_queue_t q = dispatch_get_main_queue();

    auto install = [&](int sig, __strong dispatch_source_t& dst) {
        dst = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, sig, 0, q);
        dispatch_source_set_event_handler(dst, ^{
            spdlog::info("Received signal {}, requesting shutdown", sig);
            if (g_shutdown_flag) g_shutdown_flag->store(true, std::memory_order_release);
            if ([NSApplication sharedApplication] && NSApp) {
                [NSApp stop:nil];
                // Wake up NSApp.run() — stop: only unwinds after the next event.
                NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                                  location:NSZeroPoint
                                             modifierFlags:0
                                                 timestamp:0
                                              windowNumber:0
                                                   context:nil
                                                   subtype:0
                                                     data1:0
                                                     data2:0];
                [NSApp postEvent:wake atStart:YES];
            }
        });
        dispatch_resume(dst);
    };

    install(SIGINT,  g_sigint_src);
    install(SIGTERM, g_sigterm_src);
}

void aw_macos_teardown_signals() {
    auto cancel = [](__strong dispatch_source_t& src) {
        if (src) {
            dispatch_source_cancel(src);
            src = nullptr;
        }
    };
    cancel(g_sigint_src);
    cancel(g_sigterm_src);
    g_shutdown_flag = nullptr;
}

// Run the AppKit event loop on the current (main) thread, with the caller's
// work happening on a worker thread. Blocks until aw_macos_stop_event_loop()
// (or a dispatch-source signal handler) stops NSApp.
void aw_macos_run_event_loop(std::function<void()> worker) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];

        std::thread t(std::move(worker));

        [app run];

        if (t.joinable()) t.join();
    }
}

// Ask NSApp to return from [NSApp run] on the next turn of the loop.
// Posts a dummy event so `stop:` actually takes effect if no other events
// are coming in.
void aw_macos_stop_event_loop() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!NSApp) return;
        [NSApp stop:nil];
        NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                          location:NSZeroPoint
                                     modifierFlags:0
                                         timestamp:0
                                      windowNumber:0
                                           context:nil
                                           subtype:0
                                             data1:0
                                             data2:0];
        [NSApp postEvent:wake atStart:YES];
    });
}

} // namespace autowhisper

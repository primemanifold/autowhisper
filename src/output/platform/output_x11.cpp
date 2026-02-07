#include "output/output.h"

#include <spdlog/spdlog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <cstring>
#include <string>

namespace autowhisper {

struct OutputManager::Impl {
    // No persistent state needed for X11 output
};

OutputManager::OutputManager(const OutputConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {}

OutputManager::~OutputManager() = default;

bool OutputManager::inject_platform(const std::string& text) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        spdlog::warn("Failed to open X11 display for injection");
        return false;
    }

    // Get focused window
    Window focus;
    int revert;
    XGetInputFocus(dpy, &focus, &revert);

    if (focus == None || focus == PointerRoot) {
        spdlog::warn("No focused window for X11 injection");
        XCloseDisplay(dpy);
        return false;
    }

    KeyCode shift_code = XKeysymToKeycode(dpy, XK_Shift_L);

    for (char ch : text) {
        KeySym keysym;
        bool need_shift = false;

        if (ch == '\n') {
            keysym = XK_Return;
        } else if (ch == '\t') {
            keysym = XK_Tab;
        } else {
            keysym = XStringToKeysym(std::string(1, ch).c_str());
            if (keysym == NoSymbol) {
                // Try as Unicode
                keysym = static_cast<KeySym>(ch);
            }

            // Check if shift is needed
            need_shift = (ch >= 'A' && ch <= 'Z') ||
                         std::string("~!@#$%^&*()_+{}|:\"<>?").find(ch) != std::string::npos;
        }

        KeyCode keycode = XKeysymToKeycode(dpy, keysym);
        if (keycode == 0) {
            spdlog::debug("No keycode for character: {}", ch);
            continue;
        }

        if (need_shift) {
            XTestFakeKeyEvent(dpy, shift_code, True, 0);
        }

        XTestFakeKeyEvent(dpy, keycode, True, 0);
        XTestFakeKeyEvent(dpy, keycode, False, 0);

        if (need_shift) {
            XTestFakeKeyEvent(dpy, shift_code, False, 0);
        }

        XFlush(dpy);
    }

    XSync(dpy, False);
    XCloseDisplay(dpy);

    spdlog::debug("Injected {} characters via XTest", text.size());
    return true;
}

} // namespace autowhisper

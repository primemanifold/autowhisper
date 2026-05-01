#include "output/platform_output.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <cstring>
#include <string>

namespace autowhisper {

namespace {

class X11Output : public PlatformOutput {
public:
    void initialize() override {
        xdotool_available_ = command_exists("xdotool");
        xclip_available_ = command_exists("xclip");
        xsel_available_ = command_exists("xsel");
    }

    bool inject(const std::string& text) override {
        // Try XTest first (in-process, no external process).
        if (inject_xtest(text)) return true;

        // Fall back to xdotool if available.
        if (xdotool_available_) {
            auto result = run_command({"xdotool", "type", "--clearmodifiers", "--", text}, 10);
            if (result.exit_code == 0) {
                spdlog::debug("Injected {} characters via xdotool", text.size());
                return true;
            }
            spdlog::warn("xdotool failed: {}", result.stderr_str);
        }
        return false;
    }

    bool copy_to_clipboard(const std::string& text) override {
        if (xclip_available_) {
            auto result = run_command_with_input(
                {"xclip", "-selection", "clipboard"}, text, 5);
            if (result.exit_code == 0) {
                spdlog::debug("Copied {} chars to clipboard via xclip", text.size());
                return true;
            }
            spdlog::warn("xclip failed");
        }
        if (xsel_available_) {
            auto result = run_command_with_input(
                {"xsel", "--clipboard", "--input"}, text, 5);
            if (result.exit_code == 0) {
                spdlog::debug("Copied {} chars to clipboard via xsel", text.size());
                return true;
            }
        }
        return false;
    }

    bool send_paste() override {
        if (!xdotool_available_) {
            spdlog::warn("Cannot send paste: xdotool not available");
            return false;
        }
        auto result = run_command({"xdotool", "key", "--clearmodifiers", "ctrl+v"}, 5);
        return result.exit_code == 0;
    }

    bool send_return_key() override {
        if (!xdotool_available_) {
            spdlog::warn("Cannot send Return: xdotool not available");
            return false;
        }
        auto result = run_command({"xdotool", "key", "--clearmodifiers", "Return"}, 5);
        if (result.exit_code == 0) {
            spdlog::debug("Sent Return via xdotool");
            return true;
        }
        spdlog::warn("xdotool key Return failed: {}", result.stderr_str);
        return false;
    }

    // On X11, event posting via XTest is essentially always available if
    // the X display is available. xdotool provides the same capability
    // via subprocess. Either path is enough.
    bool can_post_events() const override {
        return true;  // XOpenDisplay succeeds or xdotool succeeds; checked at call time.
    }

    bool can_copy_to_clipboard() const override {
        return xclip_available_ || xsel_available_;
    }

    std::string name() const override { return "x11"; }

private:
    bool xdotool_available_ = false;
    bool xclip_available_ = false;
    bool xsel_available_ = false;

    bool inject_xtest(const std::string& text) {
        Display* dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            spdlog::debug("XOpenDisplay failed; XTest inject unavailable");
            return false;
        }

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
                if (keysym == NoSymbol) keysym = static_cast<KeySym>(ch);
                need_shift = (ch >= 'A' && ch <= 'Z') ||
                             std::string("~!@#$%^&*()_+{}|:\"<>?").find(ch) != std::string::npos;
            }

            KeyCode keycode = XKeysymToKeycode(dpy, keysym);
            if (keycode == 0) continue;

            if (need_shift) XTestFakeKeyEvent(dpy, shift_code, True, 0);
            XTestFakeKeyEvent(dpy, keycode, True, 0);
            XTestFakeKeyEvent(dpy, keycode, False, 0);
            if (need_shift) XTestFakeKeyEvent(dpy, shift_code, False, 0);
            XFlush(dpy);
        }

        XSync(dpy, False);
        XCloseDisplay(dpy);
        spdlog::debug("Injected {} characters via XTest", text.size());
        return true;
    }
};

} // namespace

std::unique_ptr<PlatformOutput> make_platform_output() {
    return std::make_unique<X11Output>();
}

} // namespace autowhisper

#include "avatar/avatar.h"

#include <spdlog/spdlog.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <atomic>
#include <cstring>
#include <functional>
#include <thread>

namespace autowhisper {

// Pure-Xlib shell: an override-redirect ARGB window blitting the shared
// renderer at ~30fps on its own thread (no GTK, so it works with the tray
// disabled and never touches another toolkit's main loop). True per-pixel
// transparency needs a compositor — every mainstream 2026 desktop runs one;
// without one the orb sits on a dark square, which is acceptable.

struct AvatarManager::Impl {
    AvatarManager* mgr = nullptr;
    AvatarTicker* ticker = nullptr;
    std::function<void()>* toggle = nullptr;
    std::thread thread;
    std::atomic<bool> quit{false};
    int size = 96;

    void run() {
        Display* dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            spdlog::warn("avatar: cannot open X display; companion disabled");
            return;
        }
        const int scr = DefaultScreen(dpy);

        XVisualInfo vinfo;
        bool argb = XMatchVisualInfo(dpy, scr, 32, TrueColor, &vinfo);
        Visual* visual = argb ? vinfo.visual : DefaultVisual(dpy, scr);
        int depth = argb ? 32 : DefaultDepth(dpy, scr);

        XSetWindowAttributes attrs{};
        attrs.override_redirect = True;
        attrs.background_pixel = 0;
        attrs.border_pixel = 0;
        attrs.colormap = argb ? XCreateColormap(dpy, RootWindow(dpy, scr), visual,
                                                AllocNone)
                              : CopyFromParent;

        const int sw = DisplayWidth(dpy, scr), sh = DisplayHeight(dpy, scr);
        int x = sw - size - 28, y = sh - size - 96;  // bottom-right perch

        Window win = XCreateWindow(
            dpy, RootWindow(dpy, scr), x, y, unsigned(size), unsigned(size), 0,
            depth, InputOutput, visual,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap, &attrs);

        // Stay above and out of the taskbar/pager.
        Atom wtype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom wutil = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
        XChangeProperty(dpy, win, wtype, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&wutil), 1);
        XStoreName(dpy, win, "AutoWhisper companion");

        XSelectInput(dpy, win, ButtonPressMotionMask());
        XMapRaised(dpy, win);

        GC gc = XCreateGC(dpy, win, 0, nullptr);
        std::vector<uint32_t> buf(size_t(size) * size);
        XImage* img = XCreateImage(dpy, visual, unsigned(depth), ZPixmap, 0,
                                   reinterpret_cast<char*>(buf.data()),
                                   unsigned(size), unsigned(size), 32, 0);

        bool dragging = false;
        bool moved = false;
        int grab_dx = 0, grab_dy = 0, press_rx = 0, press_ry = 0;
        const AvatarCharacter& ch = *find_character(mgr->config_.character);

        while (!quit.load(std::memory_order_relaxed)) {
            while (XPending(dpy)) {
                XEvent ev;
                XNextEvent(dpy, &ev);
                if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
                    dragging = true;
                    moved = false;
                    grab_dx = ev.xbutton.x;
                    grab_dy = ev.xbutton.y;
                    press_rx = ev.xbutton.x_root;
                    press_ry = ev.xbutton.y_root;
                } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
                    // A press that never moved is a click → toggle dictation,
                    // the Wispr-Flow floating-button gesture.
                    if (!moved && toggle && *toggle) (*toggle)();
                    dragging = false;
                } else if (ev.type == MotionNotify && dragging) {
                    int dx = ev.xmotion.x_root - press_rx;
                    int dy = ev.xmotion.y_root - press_ry;
                    if (dx * dx + dy * dy > 25) moved = true;
                    XMoveWindow(dpy, win, ev.xmotion.x_root - grab_dx,
                                ev.xmotion.y_root - grab_dy);
                }
            }

            auto snap = ticker->tick();
            avatar_rasterize(buf.data(), size, ch, snap.state, snap.t, snap.phase,
                             snap.level);
            if (!argb) {
                // 24-bit visual: the server ignores alpha, so composite the
                // premultiplied buffer over a dark plate ourselves — glows
                // and feathered edges must survive, not just opaque pixels.
                for (auto& px : buf) {
                    uint32_t a = px >> 24, inv = 255 - a;
                    uint32_t r = ((px >> 16) & 0xff) + (0x16 * inv + 127) / 255;
                    uint32_t g = ((px >> 8) & 0xff) + (0x16 * inv + 127) / 255;
                    uint32_t b = (px & 0xff) + (0x17 * inv + 127) / 255;
                    px = 0xff000000 | (r << 16) | (g << 8) | b;
                }
            }
            XPutImage(dpy, win, gc, img, 0, 0, 0, 0, unsigned(size), unsigned(size));
            XFlush(dpy);
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        img->data = nullptr;  // buffer is ours, not Xlib's
        XDestroyImage(img);
        XFreeGC(dpy, gc);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
    }

    static long ButtonPressMotionMask() {
        return ButtonPressMask | ButtonReleaseMask | Button1MotionMask |
               ExposureMask;
    }
};

AvatarManager::AvatarManager(const AvatarConfig& config, LevelProvider level,
                             AmbientProvider ambient)
    : config_(config), level_(std::move(level)), ambient_(std::move(ambient)),
      impl_(std::make_unique<Impl>()) {
    impl_->mgr = this;
    impl_->size = config_.size;
}

AvatarManager::~AvatarManager() { stop(); }

void AvatarManager::start() {
    if (!config_.enabled || running_.exchange(true)) return;
    ticker_ = std::make_unique<AvatarTicker>(AvatarStateMachine{}, level_, ambient_);
    impl_->ticker = ticker_.get();
    impl_->toggle = toggle_ ? &toggle_ : nullptr;
    impl_->quit.store(false);
    impl_->thread = std::thread([this] { impl_->run(); });
    spdlog::info("avatar: {} joins ({})", find_character(config_.character)->name,
                 find_character(config_.character)->epithet);
}

void AvatarManager::stop() {
    if (!running_.exchange(false)) return;
    impl_->quit.store(true);
    if (impl_->thread.joinable()) impl_->thread.join();
}

void AvatarManager::set_state(AvatarState s) {
    if (running_.load() && impl_->ticker) impl_->ticker->request(s);
}

} // namespace autowhisper

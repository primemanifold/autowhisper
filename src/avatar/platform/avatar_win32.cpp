#include "avatar/avatar.h"

#include <spdlog/spdlog.h>

#include <windows.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace autowhisper {

// Win32 shell: a topmost layered tool window. UpdateLayeredWindow consumes
// exactly the premultiplied BGRA the shared renderer emits, so Windows
// shows the same pixels as X11 and macOS. Runs its own message-pump thread.

namespace {
constexpr wchar_t kClassName[] = L"AutoWhisperAvatar";
constexpr UINT_PTR kTimerId = 1;

struct WindowState {
    AvatarTicker* ticker = nullptr;
    const AvatarCharacter* character = nullptr;
    int size = 96;
    HDC mem_dc = nullptr;
    HBITMAP dib = nullptr;
    uint32_t* pixels = nullptr;

    void paint(HWND hwnd) {
        auto snap = ticker->tick();
        avatar_rasterize(pixels, size, *character, snap.state, snap.t, snap.phase,
                         snap.level);

        RECT r;
        GetWindowRect(hwnd, &r);
        POINT dst{r.left, r.top};
        POINT src{0, 0};
        SIZE  sz{size, size};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        HDC screen = GetDC(nullptr);
        UpdateLayeredWindow(hwnd, screen, &dst, &sz, mem_dc, &src, 0, &blend,
                            ULW_ALPHA);
        ReleaseDC(nullptr, screen);
    }
};

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_TIMER:
            if (st) st->paint(hwnd);
            return 0;
        case WM_NCHITTEST:
            // The whole orb is a drag handle.
            return HTCAPTION;
        case WM_DESTROY:
            KillTimer(hwnd, kTimerId);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
}  // namespace

struct AvatarManager::Impl {
    AvatarManager* mgr = nullptr;
    AvatarTicker* ticker = nullptr;
    std::thread thread;
    std::atomic<HWND> hwnd{nullptr};
    int size = 96;

    void run() {
        WNDCLASSW wc{};
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512) /* IDC_ARROW */);
        RegisterClassW(&wc);

        const int sw = GetSystemMetrics(SM_CXSCREEN);
        const int sh = GetSystemMetrics(SM_CYSCREEN);

        HWND w = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kClassName, L"AutoWhisper companion", WS_POPUP, sw - size - 28,
            sh - size - 110, size, size, nullptr, nullptr, wc.hInstance, nullptr);
        if (!w) {
            spdlog::warn("avatar: CreateWindowEx failed ({})", GetLastError());
            return;
        }

        WindowState st;
        st.ticker = ticker;
        st.character = find_character(mgr->config_.character);
        st.size = size;

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = size;
        bmi.bmiHeader.biHeight = -size;  // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        HDC screen = GetDC(nullptr);
        st.mem_dc = CreateCompatibleDC(screen);
        void* bits = nullptr;
        st.dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screen);
        if (!st.dib || !bits) {
            spdlog::warn("avatar: CreateDIBSection failed");
            DestroyWindow(w);
            return;
        }
        SelectObject(st.mem_dc, st.dib);
        st.pixels = static_cast<uint32_t*>(bits);

        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
        st.paint(w);
        ShowWindow(w, SW_SHOWNOACTIVATE);
        SetTimer(w, kTimerId, 33, nullptr);
        hwnd.store(w);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        hwnd.store(nullptr);
        DeleteDC(st.mem_dc);
        DeleteObject(st.dib);
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
    impl_->thread = std::thread([this] { impl_->run(); });
    spdlog::info("avatar: {} joins ({})", find_character(config_.character)->name,
                 find_character(config_.character)->epithet);
}

void AvatarManager::stop() {
    if (!running_.exchange(false)) return;
    if (HWND w = impl_->hwnd.load()) {
        PostMessageW(w, WM_CLOSE, 0, 0);
    }
    if (impl_->thread.joinable()) impl_->thread.join();
}

void AvatarManager::set_state(AvatarState s) {
    if (running_.load() && impl_->ticker) impl_->ticker->request(s);
}

} // namespace autowhisper

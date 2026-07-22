#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#include <windows.h>

#include <atomic>
#include <string>
#include <thread>

namespace autowhisper {

// Win32 global hotkeys via a low-level keyboard hook (issue #14).
// RegisterHotKey cannot deliver key-release events, which push-to-talk
// needs, and cannot express modifier-only chords like "ctrl+alt" — so a
// WH_KEYBOARD_LL hook on a dedicated message-pump thread mirrors the
// X11/XRecord and macOS/CGEventTap designs. The vk→name mapping lives in
// pure functions so the MSVC CI job (and Wine locally) can unit-test it.

std::string win32_vk_to_modifier(unsigned vk) {
    switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "shift";
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "ctrl";
        case VK_MENU: case VK_LMENU: case VK_RMENU: return "alt";
        case VK_LWIN: case VK_RWIN: return "super";
        default: return "";
    }
}

std::string win32_vk_to_name(unsigned vk) {
    if (vk >= 'A' && vk <= 'Z') return std::string(1, char('a' + (vk - 'A')));
    if (vk >= '0' && vk <= '9') return std::string(1, char(vk));
    if (vk >= VK_F1 && vk <= VK_F24) {
        return "f" + std::to_string(vk - VK_F1 + 1);
    }
    switch (vk) {
        case VK_ESCAPE: return "esc";
        case VK_SPACE: return "space";
        case VK_RETURN: return "return";
        case VK_TAB: return "tab";
        case VK_BACK: return "backspace";
        case VK_DELETE: return "delete";
        case VK_UP: return "up";
        case VK_DOWN: return "down";
        case VK_LEFT: return "left";
        case VK_RIGHT: return "right";
        default: return "";
    }
}

struct HotkeyManager::Impl {
    HotkeyManager* manager = nullptr;
    std::thread thread;
    std::atomic<DWORD> thread_id{0};

    // One listener per process — the same constraint the X11 backend has;
    // the hook callback carries no user data pointer.
    static HotkeyManager* g_manager;

    static LRESULT CALLBACK keyboard_hook(int code, WPARAM wparam, LPARAM lparam) {
        HotkeyManager* mgr = g_manager;
        if (code == HC_ACTION && mgr && mgr->running_.load()) {
            const auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
            const bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
            const bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
            if (down || up) {
                std::string mod = win32_vk_to_modifier(kb->vkCode);
                if (!mod.empty()) {
                    down ? mgr->on_modifier_press(mod) : mgr->on_modifier_release(mod);
                } else {
                    std::string name = win32_vk_to_name(kb->vkCode);
                    if (!name.empty()) {
                        down ? mgr->on_key_press(name) : mgr->on_key_release(name);
                    }
                }
            }
        }
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }
};

HotkeyManager* HotkeyManager::Impl::g_manager = nullptr;

HotkeyManager::HotkeyManager(const HotkeyConfig& config, EventCallback callback)
    : config_(config),
      callback_(std::move(callback)),
      impl_(std::make_unique<Impl>()) {
    impl_->manager = this;
    for (const auto& t : config.trigger) trigger_combos_.push_back(KeyCombo::parse(t));
    for (const auto& t : config.ask_trigger) ask_trigger_combos_.push_back(KeyCombo::parse(t));
    for (const auto& c : config.cancel) cancel_combos_.push_back(KeyCombo::parse(c));
}

HotkeyManager::~HotkeyManager() { stop(); }

void HotkeyManager::start() {
    if (running_.exchange(true)) {
        spdlog::warn("Hotkey listener already running");
        return;
    }
    Impl::g_manager = this;

    impl_->thread = std::thread([this] {
        impl_->thread_id.store(GetCurrentThreadId());
        HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, Impl::keyboard_hook,
                                       GetModuleHandleW(nullptr), 0);
        if (!hook) {
            spdlog::error("Win32 hotkey: SetWindowsHookEx failed ({})", GetLastError());
            running_.store(false);
            return;
        }
        spdlog::info("Win32 hotkey listener active (low-level keyboard hook)");

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        UnhookWindowsHookEx(hook);
    });
}

void HotkeyManager::signal_stop() {
    if (!running_.load()) return;
    if (DWORD tid = impl_->thread_id.load()) {
        PostThreadMessageW(tid, WM_QUIT, 0, 0);
    }
}

void HotkeyManager::stop() {
    running_.store(false);
    if (DWORD tid = impl_->thread_id.load()) {
        PostThreadMessageW(tid, WM_QUIT, 0, 0);
    }
    if (impl_->thread.joinable()) impl_->thread.join();
    impl_->thread_id.store(0);
    if (Impl::g_manager == this) Impl::g_manager = nullptr;

    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

} // namespace autowhisper

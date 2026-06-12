// Windows text output: SendInput Unicode injection + clipboard, mirroring
// the X11 (XTest/xclip) and macOS (CGEventPost/NSPasteboard) backends so the
// daemon's inject -> clipboard fallback chain behaves identically (issue #14).
#include "output/platform_output.h"

#include <windows.h>

#include <memory>
#include <string>

namespace autowhisper {

namespace {

// UTF-8 -> UTF-16, the encoding every Win32 text API speaks.
std::wstring to_utf16(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n);
    return out;
}

void send_unicode_unit(WORD unit, bool key_up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = unit;
    in.ki.dwFlags = KEYEVENTF_UNICODE | (key_up ? KEYEVENTF_KEYUP : 0);
    SendInput(1, &in, sizeof(INPUT));
}

void send_vk(WORD vk, bool key_up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = key_up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &in, sizeof(INPUT));
}

class Win32Output : public PlatformOutput {
public:
    bool inject(const std::string& text) override {
        std::wstring wide = to_utf16(text);
        if (wide.empty() && !text.empty()) return false;
        // Each UTF-16 code unit (including surrogate halves) is one keystroke,
        // which is exactly how KEYEVENTF_UNICODE wants astral characters.
        for (wchar_t wc : wide) {
            send_unicode_unit(WORD(wc), false);
            send_unicode_unit(WORD(wc), true);
        }
        return true;
    }

    bool copy_to_clipboard(const std::string& text) override {
        std::wstring wide = to_utf16(text);
        if (!OpenClipboard(nullptr)) return false;
        EmptyClipboard();
        size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!mem) { CloseClipboard(); return false; }
        void* dst = GlobalLock(mem);
        memcpy(dst, wide.c_str(), bytes);
        GlobalUnlock(mem);
        bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
        if (!ok) GlobalFree(mem);  // ownership only transfers on success
        CloseClipboard();
        return ok;
    }

    bool send_paste() override {
        send_vk(VK_CONTROL, false);
        send_vk('V', false);
        send_vk('V', true);
        send_vk(VK_CONTROL, true);
        return true;
    }

    bool send_return_key() override {
        send_vk(VK_RETURN, false);
        send_vk(VK_RETURN, true);
        return true;
    }

    bool can_post_events() const override { return true; }
    bool can_copy_to_clipboard() const override { return true; }

    std::string name() const override { return "win32"; }
};

} // namespace

std::unique_ptr<PlatformOutput> make_platform_output() {
    return std::make_unique<Win32Output>();
}

} // namespace autowhisper

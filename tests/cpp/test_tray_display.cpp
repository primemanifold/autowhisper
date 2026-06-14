#include <catch2/catch_test_macros.hpp>

#include "tray/tray.h"

using namespace autowhisper;

// ============================================================
// display_key
// ============================================================

TEST_CASE("display_key converts modifier names", "[tray][display]") {
    // macOS shows the standard glyphs; other platforms show words.
#ifdef __APPLE__
    CHECK(display_key("shift") == "\xE2\x87\xA7");  // ⇧
    CHECK(display_key("ctrl") == "\xE2\x8C\x83");   // ⌃
    CHECK(display_key("alt") == "\xE2\x8C\xA5");    // ⌥
    CHECK(display_key("super") == "\xE2\x8C\x98");  // ⌘
#else
    CHECK(display_key("shift") == "Shift");
    CHECK(display_key("ctrl") == "Ctrl");
    CHECK(display_key("alt") == "Alt");
    CHECK(display_key("super") == "Super");
#endif
}

TEST_CASE("display_key converts special keys", "[tray][display]") {
    CHECK(display_key("esc") == "Esc");
    CHECK(display_key("enter") == "Enter");
    CHECK(display_key("space") == "Space");
    CHECK(display_key("tab") == "Tab");
}

TEST_CASE("display_key handles media keys", "[tray][display]") {
    CHECK(display_key("media_play_pause") == "Play/Pause");
    CHECK(display_key("media_volume_mute") == "Mute");
    CHECK(display_key("media_volume_up") == "Vol+");
    CHECK(display_key("media_volume_down") == "Vol-");
    CHECK(display_key("media_next") == "Next");
    CHECK(display_key("media_previous") == "Prev");
}

TEST_CASE("display_key capitalizes single characters", "[tray][display]") {
    CHECK(display_key("a") == "A");
    CHECK(display_key("z") == "Z");
    CHECK(display_key("m") == "M");
}

TEST_CASE("display_key capitalizes first letter of unknown keys", "[tray][display]") {
    CHECK(display_key("backspace") == "Backspace");
    CHECK(display_key("delete") == "Delete");
    CHECK(display_key("f1") == "F1");
}

// ============================================================
// display_hotkey
// ============================================================

TEST_CASE("display_hotkey formats compound hotkeys", "[tray][display]") {
    CHECK(display_hotkey("ctrl+shift+space") == "Ctrl+Shift+Space");
    CHECK(display_hotkey("shift+super") == "Shift+Super");
    CHECK(display_hotkey("alt+f4") == "Alt+F4");
    CHECK(display_hotkey("esc") == "Esc");
    CHECK(display_hotkey("") == "");
}

// ============================================================
// format_hotkeys
// ============================================================

TEST_CASE("format_hotkeys formats vector of hotkeys", "[tray][display]") {
    CHECK(format_hotkeys({}) == "(none)");
    CHECK(format_hotkeys({"shift+super"}) == "Shift+Super");
    CHECK(format_hotkeys({"shift+super", "ctrl+space"}) == "Shift+Super, Ctrl+Space");
}

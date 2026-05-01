// Pure-function coverage of the macOS virtual-keycode → canonical-name map.
// Compiled only on APPLE — the functions under test live in hotkey_macos.mm.

#if defined(__APPLE__)

#include <catch2/catch_test_macros.hpp>
#include <Carbon/Carbon.h>  // for kVK_* constants
#include <ApplicationServices/ApplicationServices.h>  // for kCGEventFlagMask*
#include <cstdint>
#include <string>

namespace autowhisper {
std::string macos_keycode_to_name(uint16_t vk);
std::string macos_flag_bit_to_modifier(uint64_t bit);
}

using autowhisper::macos_keycode_to_name;
using autowhisper::macos_flag_bit_to_modifier;

TEST_CASE("letters map to lowercase canonical names") {
    REQUIRE(macos_keycode_to_name(kVK_ANSI_A) == "a");
    REQUIRE(macos_keycode_to_name(kVK_ANSI_Z) == "z");
    REQUIRE(macos_keycode_to_name(kVK_ANSI_M) == "m");
}

TEST_CASE("digits map to their character") {
    REQUIRE(macos_keycode_to_name(kVK_ANSI_0) == "0");
    REQUIRE(macos_keycode_to_name(kVK_ANSI_9) == "9");
}

TEST_CASE("whitespace/nav canonical names match X11 backend") {
    REQUIRE(macos_keycode_to_name(kVK_Space)  == "space");
    REQUIRE(macos_keycode_to_name(kVK_Return) == "return");
    REQUIRE(macos_keycode_to_name(kVK_Tab)    == "tab");
    REQUIRE(macos_keycode_to_name(kVK_Escape) == "esc");
}

TEST_CASE("function keys are lowercase f1..f12") {
    REQUIRE(macos_keycode_to_name(kVK_F1)  == "f1");
    REQUIRE(macos_keycode_to_name(kVK_F12) == "f12");
}

TEST_CASE("unknown keycodes yield empty string") {
    // 0xFF is not a real kVK constant.
    REQUIRE(macos_keycode_to_name(0xFF).empty());
}

TEST_CASE("modifier flag bits map to X11-compatible names") {
    REQUIRE(macos_flag_bit_to_modifier(kCGEventFlagMaskShift)     == "shift");
    REQUIRE(macos_flag_bit_to_modifier(kCGEventFlagMaskControl)   == "ctrl");
    REQUIRE(macos_flag_bit_to_modifier(kCGEventFlagMaskAlternate) == "alt");
    // Command key is exposed as "super" to match the Linux config.
    REQUIRE(macos_flag_bit_to_modifier(kCGEventFlagMaskCommand)   == "super");
}

TEST_CASE("unknown modifier bits yield empty string") {
    REQUIRE(macos_flag_bit_to_modifier(0).empty());
    REQUIRE(macos_flag_bit_to_modifier(0x1u << 30).empty());
}

#endif // __APPLE__

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "hotkey/hotkey.h"

using namespace autowhisper;
using Catch::Matchers::ContainsSubstring;

// ============================================================
// Modifier-only combos
// ============================================================

TEST_CASE("KeyCombo::parse handles modifier-only combos", "[hotkey][parse]") {
    SECTION("shift+super") {
        auto combo = KeyCombo::parse("shift+super");
        CHECK(combo.is_modifier_only == true);
        CHECK(combo.key.empty());
        CHECK(combo.modifiers.count("shift") == 1);
        CHECK(combo.modifiers.count("super") == 1);
        CHECK(combo.modifiers.size() == 2);
    }

    SECTION("ctrl+alt") {
        auto combo = KeyCombo::parse("ctrl+alt");
        CHECK(combo.is_modifier_only == true);
        CHECK(combo.key.empty());
        CHECK(combo.modifiers.count("ctrl") == 1);
        CHECK(combo.modifiers.count("alt") == 1);
    }

    SECTION("single modifier") {
        auto combo = KeyCombo::parse("shift");
        CHECK(combo.is_modifier_only == true);
        CHECK(combo.key.empty());
        CHECK(combo.modifiers.size() == 1);
        CHECK(combo.modifiers.count("shift") == 1);
    }

    SECTION("all four modifiers") {
        auto combo = KeyCombo::parse("ctrl+alt+shift+super");
        CHECK(combo.is_modifier_only == true);
        CHECK(combo.modifiers.size() == 4);
    }
}

// ============================================================
// Modifier+key combos
// ============================================================

TEST_CASE("KeyCombo::parse handles modifier+key combos", "[hotkey][parse]") {
    SECTION("ctrl+shift+space") {
        auto combo = KeyCombo::parse("ctrl+shift+space");
        CHECK(combo.is_modifier_only == false);
        CHECK(combo.key == "space");
        CHECK(combo.modifiers.count("ctrl") == 1);
        CHECK(combo.modifiers.count("shift") == 1);
        CHECK(combo.modifiers.size() == 2);
    }

    SECTION("ctrl+c") {
        auto combo = KeyCombo::parse("ctrl+c");
        CHECK(combo.is_modifier_only == false);
        CHECK(combo.key == "c");
        CHECK(combo.modifiers.size() == 1);
        CHECK(combo.modifiers.count("ctrl") == 1);
    }

    SECTION("alt+f4") {
        auto combo = KeyCombo::parse("alt+f4");
        CHECK(combo.is_modifier_only == false);
        CHECK(combo.key == "f4");
        CHECK(combo.modifiers.count("alt") == 1);
    }
}

// ============================================================
// Standalone keys
// ============================================================

TEST_CASE("KeyCombo::parse handles standalone keys", "[hotkey][parse]") {
    SECTION("esc") {
        auto combo = KeyCombo::parse("esc");
        CHECK(combo.is_modifier_only == false);
        CHECK(combo.key == "esc");
        CHECK(combo.modifiers.empty());
    }

    SECTION("space") {
        auto combo = KeyCombo::parse("space");
        CHECK(combo.is_modifier_only == false);
        CHECK(combo.key == "space");
        CHECK(combo.modifiers.empty());
    }

    SECTION("f1") {
        auto combo = KeyCombo::parse("f1");
        CHECK(combo.is_modifier_only == false);
        CHECK(combo.key == "f1");
        CHECK(combo.modifiers.empty());
    }
}

// ============================================================
// Modifier normalization
// ============================================================

TEST_CASE("KeyCombo::parse normalizes modifier names", "[hotkey][parse]") {
    SECTION("control -> ctrl") {
        auto combo = KeyCombo::parse("control+space");
        CHECK(combo.modifiers.count("ctrl") == 1);
        CHECK(combo.modifiers.count("control") == 0);
    }

    SECTION("win -> super") {
        auto combo = KeyCombo::parse("win+space");
        CHECK(combo.modifiers.count("super") == 1);
    }

    SECTION("cmd -> super") {
        auto combo = KeyCombo::parse("cmd+space");
        CHECK(combo.modifiers.count("super") == 1);
    }

    SECTION("meta -> super") {
        auto combo = KeyCombo::parse("meta+space");
        CHECK(combo.modifiers.count("super") == 1);
    }

    SECTION("option -> alt") {
        auto combo = KeyCombo::parse("option+space");
        CHECK(combo.modifiers.count("alt") == 1);
    }
}

// ============================================================
// Case insensitivity
// ============================================================

TEST_CASE("KeyCombo::parse handles case insensitivity", "[hotkey][parse]") {
    SECTION("mixed case modifier+key") {
        auto combo = KeyCombo::parse("Ctrl+Shift+SPACE");
        CHECK(combo.modifiers.count("ctrl") == 1);
        CHECK(combo.modifiers.count("shift") == 1);
        CHECK(combo.key == "space");
    }

    SECTION("uppercase modifier-only") {
        auto combo = KeyCombo::parse("SHIFT+SUPER");
        CHECK(combo.is_modifier_only == true);
        CHECK(combo.modifiers.count("shift") == 1);
        CHECK(combo.modifiers.count("super") == 1);
    }
}

// ============================================================
// Whitespace trimming
// ============================================================

TEST_CASE("KeyCombo::parse handles whitespace trimming", "[hotkey][parse]") {
    SECTION("spaces around +") {
        auto combo = KeyCombo::parse("ctrl + shift + space");
        CHECK(combo.modifiers.count("ctrl") == 1);
        CHECK(combo.modifiers.count("shift") == 1);
        CHECK(combo.key == "space");
    }

    SECTION("leading/trailing spaces") {
        auto combo = KeyCombo::parse(" shift + super ");
        CHECK(combo.is_modifier_only == true);
        CHECK(combo.modifiers.count("shift") == 1);
        CHECK(combo.modifiers.count("super") == 1);
    }
}

// ============================================================
// Error cases
// ============================================================

TEST_CASE("KeyCombo::parse throws on empty string", "[hotkey][parse]") {
    REQUIRE_THROWS_WITH(KeyCombo::parse(""), ContainsSubstring("Invalid key combination"));
}

TEST_CASE("KeyCombo::parse throws when non-modifiers appear before the key", "[hotkey][parse]") {
    REQUIRE_THROWS_WITH(KeyCombo::parse("a+b"), ContainsSubstring("Invalid key combination"));
    REQUIRE_THROWS_WITH(KeyCombo::parse("ctrl+a+b"), ContainsSubstring("Invalid key combination"));
}

// ============================================================
// Edge cases
// ============================================================

TEST_CASE("KeyCombo::parse deduplicates modifiers via set", "[hotkey][parse]") {
    // "ctrl+ctrl+space" - first two parts normalize to "ctrl",
    // but only last non-modifier becomes the key.
    // Since "ctrl" appears twice as modifiers and "space" is the key:
    // Actually, the algorithm treats last part as key if not all modifiers.
    // Parts: ["ctrl", "ctrl", "space"]
    // Normalized: ["ctrl", "ctrl", "space"]
    // Not all modifiers (space is not), so modifiers = first N-1, key = last
    // modifiers = {"ctrl", "ctrl"} -> set = {"ctrl"}, key = "space"
    auto combo = KeyCombo::parse("ctrl+ctrl+space");
    CHECK(combo.modifiers.size() == 1);
    CHECK(combo.modifiers.count("ctrl") == 1);
    CHECK(combo.key == "space");
}

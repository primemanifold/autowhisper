#include "platform/capabilities.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

using namespace autowhisper;

TEST_CASE("platform capabilities identify the compiled desktop target", "[platform]") {
    const auto caps = current_platform_capabilities();

    REQUIRE_FALSE(caps.id.empty());
    REQUIRE_FALSE(caps.display_name.empty());
    REQUIRE(caps.buildable);
    REQUIRE_FALSE(caps.features.empty());

#if defined(__APPLE__)
    REQUIRE(caps.id == "macos");
    REQUIRE(caps.display_name == "macOS");
#elif defined(_WIN32)
    REQUIRE(caps.id == "windows");
    REQUIRE(caps.display_name == "Windows");
#elif defined(__linux__)
    REQUIRE(caps.id == "linux");
    REQUIRE(caps.display_name == "Linux/X11");
#endif
}

TEST_CASE("platform capabilities are honest about placeholder desktop ports", "[platform]") {
    const auto caps = current_platform_capabilities();
    const auto find_feature = [&](const std::string& id) -> const PlatformFeature* {
        const auto it = std::find_if(caps.features.begin(), caps.features.end(), [&](const PlatformFeature& feature) {
            return feature.id == id;
        });
        return it == caps.features.end() ? nullptr : &*it;
    };

    const auto* hotkey = find_feature("global_hotkey");
    const auto* output = find_feature("text_insertion");
    const auto* tray = find_feature("tray_or_menu_bar");

    REQUIRE(hotkey != nullptr);
    REQUIRE(output != nullptr);
    REQUIRE(tray != nullptr);

#if defined(__APPLE__) || defined(_WIN32)
    REQUIRE(hotkey->state == PlatformFeatureState::Placeholder);
    REQUIRE(output->state == PlatformFeatureState::Placeholder);
    REQUIRE(tray->state == PlatformFeatureState::Placeholder);
    REQUIRE_FALSE(hotkey->operator_note.empty());
    REQUIRE_FALSE(output->operator_note.empty());
    REQUIRE_FALSE(tray->operator_note.empty());
#elif defined(__linux__)
    REQUIRE(hotkey->state == PlatformFeatureState::Ready);
    REQUIRE(output->state == PlatformFeatureState::Ready);
#endif
}

TEST_CASE("platform capabilities serialize to stable JSON for settings diagnostics", "[platform]") {
    const auto json = platform_capabilities_json(current_platform_capabilities());

    REQUIRE(json.at("id").is_string());
    REQUIRE(json.at("display_name").is_string());
    REQUIRE(json.at("buildable").is_boolean());
    REQUIRE(json.at("summary").is_string());
    REQUIRE(json.at("features").is_array());
    REQUIRE_FALSE(json.at("features").empty());

    for (const auto& feature : json.at("features")) {
        REQUIRE(feature.at("id").is_string());
        REQUIRE(feature.at("label").is_string());
        REQUIRE(feature.at("state").is_string());
        REQUIRE(feature.at("operator_note").is_string());
    }
}

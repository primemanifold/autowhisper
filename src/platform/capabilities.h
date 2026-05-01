#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

namespace autowhisper {

enum class PlatformFeatureState {
    Ready,
    Partial,
    Placeholder,
    Unsupported,
};

struct PlatformFeature {
    std::string id;
    std::string label;
    PlatformFeatureState state;
    std::string operator_note;
};

struct PlatformCapabilities {
    std::string id;
    std::string display_name;
    bool buildable = false;
    std::string summary;
    std::vector<PlatformFeature> features;
};

const char* platform_feature_state_name(PlatformFeatureState state) noexcept;
PlatformCapabilities current_platform_capabilities();
nlohmann::json platform_capabilities_json(const PlatformCapabilities& capabilities);

}  // namespace autowhisper

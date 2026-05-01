#include "platform/capabilities.h"

#include <nlohmann/json.hpp>

namespace autowhisper {

const char* platform_feature_state_name(PlatformFeatureState state) noexcept {
    switch (state) {
    case PlatformFeatureState::Ready:
        return "ready";
    case PlatformFeatureState::Partial:
        return "partial";
    case PlatformFeatureState::Placeholder:
        return "placeholder";
    case PlatformFeatureState::Unsupported:
        return "unsupported";
    }
    return "unknown";
}

nlohmann::json platform_capabilities_json(const PlatformCapabilities& capabilities) {
    nlohmann::json features = nlohmann::json::array();
    for (const auto& feature : capabilities.features) {
        features.push_back({
            {"id", feature.id},
            {"label", feature.label},
            {"name", feature.label},
            {"state", platform_feature_state_name(feature.state)},
            {"operator_note", feature.operator_note},
            {"detail", feature.operator_note},
        });
    }

    return nlohmann::json{
        {"id", capabilities.id},
        {"platform", capabilities.id},
        {"display_name", capabilities.display_name},
        {"build_target", capabilities.display_name},
        {"buildable", capabilities.buildable},
        {"summary", capabilities.summary},
        {"features", std::move(features)},
    };
}

PlatformCapabilities current_platform_capabilities() {
#if defined(__APPLE__)
    return PlatformCapabilities{
        "macos",
        "macOS",
        true,
        "This build compiles on macOS, but core desktop integrations are still placeholder-level until the native menu bar, permissions, hotkey, and insertion slices land.",
        {
            {"audio_capture", "Audio capture", PlatformFeatureState::Partial,
             "miniaudio can compile on macOS; a release app still needs an explicit microphone permission/onboarding flow."},
            {"global_hotkey", "Global hotkey", PlatformFeatureState::Placeholder,
             "macOS hotkey capture is not implemented yet; the current platform file only logs an unsupported warning."},
            {"text_insertion", "Text insertion", PlatformFeatureState::Placeholder,
             "macOS-native insertion and clipboard fallback are not implemented yet."},
            {"tray_or_menu_bar", "Menu bar", PlatformFeatureState::Placeholder,
             "NSStatusItem/menu-bar behavior is not implemented yet."},
            {"service_integration", "Launch/service integration", PlatformFeatureState::Unsupported,
             "Linux systemd commands do not apply on macOS; launchd packaging is a later slice."},
        },
    };
#elif defined(_WIN32)
    return PlatformCapabilities{
        "windows",
        "Windows",
        true,
        "The Windows target is intended to cross-compile first; runtime desktop integrations remain placeholder-level until native Win32 slices land.",
        {
            {"audio_capture", "Audio capture", PlatformFeatureState::Partial,
             "miniaudio can target Windows; real device and permission smoke tests require a Windows host or VM."},
            {"global_hotkey", "Global hotkey", PlatformFeatureState::Placeholder,
             "Win32 hotkey capture is not implemented yet; the current platform file only logs an unsupported warning."},
            {"text_insertion", "Text insertion", PlatformFeatureState::Placeholder,
             "Win32 SendInput/clipboard fallback is not implemented yet."},
            {"tray_or_menu_bar", "System tray", PlatformFeatureState::Placeholder,
             "Shell_NotifyIcon/system-tray behavior is not implemented yet."},
            {"service_integration", "Startup/service integration", PlatformFeatureState::Unsupported,
             "Windows service/startup integration and installer support are later slices."},
        },
    };
#elif defined(__linux__)
    return PlatformCapabilities{
        "linux",
        "Linux/X11",
        true,
        "Linux/X11 remains the currently implemented desktop product path.",
        {
            {"audio_capture", "Audio capture", PlatformFeatureState::Ready,
             "PulseAudio/miniaudio path is implemented for the current Linux target."},
            {"global_hotkey", "Global hotkey", PlatformFeatureState::Ready,
             "X11 global hotkey support is implemented."},
            {"text_insertion", "Text insertion", PlatformFeatureState::Ready,
             "X11 text insertion and clipboard tooling are implemented for supported Linux desktops."},
            {"tray_or_menu_bar", "Tray", PlatformFeatureState::Ready,
             "GTK/AppIndicator tray support is implemented when dependencies are present."},
            {"service_integration", "systemd user service", PlatformFeatureState::Ready,
             "systemd user service commands are implemented for Linux."},
        },
    };
#else
    return PlatformCapabilities{
        "unknown",
        "Unknown",
        false,
        "This platform is not a supported AutoWhisper target.",
        {
            {"audio_capture", "Audio capture", PlatformFeatureState::Unsupported, "Unsupported platform."},
            {"global_hotkey", "Global hotkey", PlatformFeatureState::Unsupported, "Unsupported platform."},
            {"text_insertion", "Text insertion", PlatformFeatureState::Unsupported, "Unsupported platform."},
            {"tray_or_menu_bar", "Tray/menu bar", PlatformFeatureState::Unsupported, "Unsupported platform."},
            {"service_integration", "Service integration", PlatformFeatureState::Unsupported, "Unsupported platform."},
        },
    };
#endif
}

}  // namespace autowhisper

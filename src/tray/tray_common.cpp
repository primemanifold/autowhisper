#include "tray/tray.h"

#include <algorithm>
#include <sstream>

namespace autowhisper {

std::string display_key(const std::string& keyname) {
    // Modifier names are platform-localized: macOS users read ⌘/⌥/⌃/⇧, not
    // the Linux "Super". The canonical config token stays "super" everywhere;
    // only the human-facing label changes.
#ifdef __APPLE__
    if (keyname == "shift") return "⇧";        // ⇧
    if (keyname == "ctrl") return "⌃";         // ⌃
    if (keyname == "alt") return "⌥";          // ⌥
    if (keyname == "super") return "⌘";        // ⌘
#else
    if (keyname == "shift") return "Shift";
    if (keyname == "ctrl") return "Ctrl";
    if (keyname == "alt") return "Alt";
    if (keyname == "super") return "Super";
#endif
    if (keyname == "esc") return "Esc";
    if (keyname == "enter") return "Enter";
    if (keyname == "space") return "Space";
    if (keyname == "tab") return "Tab";
    if (keyname == "media_play_pause") return "Play/Pause";
    if (keyname == "media_volume_mute") return "Mute";
    if (keyname == "media_volume_up") return "Vol+";
    if (keyname == "media_volume_down") return "Vol-";
    if (keyname == "media_next") return "Next";
    if (keyname == "media_previous") return "Prev";

    if (keyname.size() == 1) {
        std::string upper(1, static_cast<char>(toupper(keyname[0])));
        return upper;
    }

    // Capitalize first letter
    std::string result = keyname;
    if (!result.empty()) {
        result[0] = static_cast<char>(toupper(result[0]));
    }
    return result;
}

std::string display_hotkey(const std::string& hotkey) {
    if (hotkey.empty()) return "";

    std::string result;
    std::istringstream iss(hotkey);
    std::string part;
    while (std::getline(iss, part, '+')) {
        if (!result.empty()) result += "+";
        result += display_key(part);
    }
    return result;
}

std::string format_hotkeys(const std::vector<std::string>& hotkeys) {
    if (hotkeys.empty()) return "(none)";
    std::string result;
    for (const auto& hk : hotkeys) {
        if (!result.empty()) result += ", ";
        result += display_hotkey(hk);
    }
    return result;
}

} // namespace autowhisper

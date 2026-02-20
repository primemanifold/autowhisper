#include "hotkey/hotkey.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>

namespace autowhisper {

KeyCombo KeyCombo::parse(const std::string& combo_str) {
    KeyCombo result;

    // Split by +
    std::vector<std::string> parts;
    std::istringstream iss(combo_str);
    std::string part;
    while (std::getline(iss, part, '+')) {
        // Lowercase and trim
        std::transform(part.begin(), part.end(), part.begin(), ::tolower);
        while (!part.empty() && part.front() == ' ') part.erase(part.begin());
        while (!part.empty() && part.back() == ' ') part.pop_back();
        if (!part.empty()) parts.push_back(part);
    }

    if (parts.empty()) {
        throw std::runtime_error("Invalid key combination: " + combo_str);
    }

    // Normalize modifier names
    auto normalize = [](const std::string& p) -> std::string {
        if (p == "ctrl" || p == "control") return "ctrl";
        if (p == "alt" || p == "option") return "alt";
        if (p == "shift") return "shift";
        if (p == "super" || p == "win" || p == "cmd" || p == "meta") return "super";
        return p;
    };

    static const std::set<std::string> valid_mods = {"ctrl", "alt", "shift", "super"};

    std::vector<std::string> normalized;
    for (const auto& p : parts) {
        normalized.push_back(normalize(p));
    }

    // Check if all parts are modifiers
    bool all_modifiers = true;
    for (const auto& p : normalized) {
        if (valid_mods.find(p) == valid_mods.end()) {
            all_modifiers = false;
            break;
        }
    }

    if (all_modifiers) {
        result.modifiers = std::set<std::string>(normalized.begin(), normalized.end());
        result.is_modifier_only = true;
    } else {
        // For modifier+key combos, every token except the last must be a modifier.
        // Otherwise combos like "a+b" silently become impossible to trigger.
        for (size_t i = 0; i + 1 < normalized.size(); ++i) {
            if (valid_mods.find(normalized[i]) == valid_mods.end()) {
                throw std::runtime_error("Invalid key combination: " + combo_str);
            }
        }

        if (normalized.size() > 1) {
            result.modifiers = std::set<std::string>(normalized.begin(), normalized.end() - 1);
        }
        result.key = normalized.back();
        result.is_modifier_only = false;
    }

    return result;
}

void HotkeyManager::send_event(HotkeyEvent event) {
    spdlog::debug("Hotkey event: {}",
                  event == HotkeyEvent::START ? "START" :
                  event == HotkeyEvent::STOP ? "STOP" : "CANCEL");
    if (callback_) {
        callback_(event);
    }
}

std::optional<KeyCombo> HotkeyManager::check_any_trigger(const std::string& key_name) const {
    for (const auto& combo : trigger_combos_) {
        if (combo.is_modifier_only) {
            if (combo.modifiers == pressed_modifiers_) {
                return combo;
            }
        } else if (!key_name.empty() && check_combo(combo, key_name)) {
            return combo;
        }
    }
    return std::nullopt;
}

bool HotkeyManager::check_any_cancel(const std::string& key_name) const {
    for (const auto& combo : cancel_combos_) {
        if (!combo.is_modifier_only && check_combo(combo, key_name)) {
            return true;
        }
    }
    return false;
}

bool HotkeyManager::check_combo(const KeyCombo& combo, const std::string& key_name) const {
    if (combo.modifiers != pressed_modifiers_) return false;

    auto normalize_alias = [](const std::string& key) -> std::string {
        if (key == "enter") return "return";
        if (key == "escape") return "esc";
        return key;
    };

    std::string combo_key = normalize_alias(combo.key);
    std::string incoming_key = normalize_alias(key_name);

    // Try exact match and underscore-removed match
    std::string key_no_underscore = combo_key;
    key_no_underscore.erase(std::remove(key_no_underscore.begin(), key_no_underscore.end(), '_'),
                             key_no_underscore.end());
    return incoming_key == combo_key || incoming_key == key_no_underscore;
}

void HotkeyManager::on_modifier_press(const std::string& modifier) {
    if (!running_.load()) return;

    pressed_modifiers_.insert(modifier);
    spdlog::debug("Mod down: {}, pressed: {}", modifier, pressed_modifiers_.size());

    // Check for modifier-only trigger
    auto matched = check_any_trigger();
    if (matched && matched->is_modifier_only && !trigger_pressed_) {
        spdlog::debug("Modifier-only trigger combo detected");
        trigger_pressed_ = true;
        active_trigger_ = *matched;
        send_event(HotkeyEvent::START);
    }
}

void HotkeyManager::on_modifier_release(const std::string& modifier) {
    if (!running_.load()) return;

    pressed_modifiers_.erase(modifier);
    spdlog::debug("Mod up: {}", modifier);

    // For push-to-talk with modifier-only combos
    if (active_trigger_ && active_trigger_->is_modifier_only &&
        config_.mode == "push_to_talk" && trigger_pressed_ &&
        active_trigger_->modifiers.count(modifier) > 0) {
        spdlog::debug("Modifier-only trigger released, stopping");
        trigger_pressed_ = false;
        active_trigger_.reset();
        send_event(HotkeyEvent::STOP);
    }
}

void HotkeyManager::on_key_press(const std::string& key_name) {
    if (!running_.load()) return;

    spdlog::debug("Key pressed: {}", key_name);

    // Check for Escape to cancel
    if (config_.escape_to_cancel && (key_name == "esc" || key_name == "escape")) {
        spdlog::debug("Escape pressed, cancelling");
        trigger_pressed_ = false;
        send_event(HotkeyEvent::CANCEL);
        return;
    }

    // Check cancel combo
    if (check_any_cancel(key_name)) {
        spdlog::debug("Cancel hotkey pressed");
        trigger_pressed_ = false;
        send_event(HotkeyEvent::CANCEL);
        return;
    }

    // Check trigger combo (non-modifier-only)
    auto matched = check_any_trigger(key_name);
    if (matched && !matched->is_modifier_only) {
        if (config_.mode == "toggle") {
            if (trigger_pressed_) {
                trigger_pressed_ = false;
                send_event(HotkeyEvent::STOP);
            } else {
                trigger_pressed_ = true;
                active_trigger_ = *matched;
                send_event(HotkeyEvent::START);
            }
        } else {
            // Push-to-talk: start on press
            if (!trigger_pressed_) {
                trigger_pressed_ = true;
                active_trigger_ = *matched;
                send_event(HotkeyEvent::START);
            }
        }
    }
}

void HotkeyManager::on_key_release(const std::string& key_name) {
    if (!running_.load()) return;

    // Push-to-talk: stop on release for non-modifier-only combos
    if (active_trigger_ && !active_trigger_->is_modifier_only &&
        config_.mode == "push_to_talk" && trigger_pressed_) {
        if (check_combo(*active_trigger_, key_name)) {
            trigger_pressed_ = false;
            active_trigger_.reset();
            send_event(HotkeyEvent::STOP);
        }
    }
}

} // namespace autowhisper

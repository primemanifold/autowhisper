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

void HotkeyManager::set_config(const HotkeyConfig& config) {
    // Parse outside the lock (KeyCombo::parse can throw); a bad combo is
    // skipped rather than crashing the live listener. The settings UI
    // validates before save, so this is defensive.
    std::vector<KeyCombo> triggers;
    std::vector<KeyCombo> ask_triggers;
    std::vector<KeyCombo> cancels;
    for (const auto& t : config.trigger) {
        try {
            triggers.push_back(KeyCombo::parse(t));
        } catch (const std::exception& e) {
            spdlog::warn("Hotkey: ignoring invalid trigger '{}': {}", t, e.what());
        }
    }
    for (const auto& t : config.ask_trigger) {
        try {
            ask_triggers.push_back(KeyCombo::parse(t));
        } catch (const std::exception& e) {
            spdlog::warn("Hotkey: ignoring invalid Ask Fabric trigger '{}': {}", t, e.what());
        }
    }
    for (const auto& c : config.cancel) {
        try {
            cancels.push_back(KeyCombo::parse(c));
        } catch (const std::exception& e) {
            spdlog::warn("Hotkey: ignoring invalid cancel '{}': {}", c, e.what());
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    trigger_combos_ = std::move(triggers);
    ask_trigger_combos_ = std::move(ask_triggers);
    cancel_combos_ = std::move(cancels);
    // A live re-config invalidates any in-flight press: drop cached state so a
    // half-held old chord cannot strand the trigger.
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
    spdlog::info("Hotkey: config applied live ({} Dictate, {} Ask Fabric, mode {})",
                 trigger_combos_.size(), ask_trigger_combos_.size(), config_.mode);
}

void HotkeyManager::send_event(HotkeyEvent event) {
    const char* name = "CANCEL";
    switch (event) {
        case HotkeyEvent::START: name = "START"; break;
        case HotkeyEvent::STOP: name = "STOP"; break;
        case HotkeyEvent::ASK_START: name = "ASK_START"; break;
        case HotkeyEvent::ASK_STOP: name = "ASK_STOP"; break;
        case HotkeyEvent::CANCEL: break;
    }
    spdlog::debug("Hotkey event: {}", name);
    if (callback_) {
        callback_(event);
    }
}

void HotkeyManager::reset_input_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::debug("Hotkey: resetting cached input state (modifier resync)");
    pressed_modifiers_.clear();
    trigger_pressed_ = false;
    active_trigger_.reset();
}

std::optional<HotkeyManager::TriggerMatch>
HotkeyManager::check_any_trigger(const std::string& key_name) const {
    auto find = [&](const std::vector<KeyCombo>& combos,
                    TriggerTarget target) -> std::optional<TriggerMatch> {
        for (const auto& combo : combos) {
            if (combo.is_modifier_only) {
                if (combo.modifiers == pressed_modifiers_) return TriggerMatch{combo, target};
            } else if (!key_name.empty() && check_combo(combo, key_name)) {
                return TriggerMatch{combo, target};
            }
        }
        return std::nullopt;
    };

    if (auto match = find(trigger_combos_, TriggerTarget::DICTATE)) return match;
    if (auto match = find(ask_trigger_combos_, TriggerTarget::ASK_FABRIC)) return match;
    return std::nullopt;
}

HotkeyEvent HotkeyManager::start_event(TriggerTarget target) {
    return target == TriggerTarget::ASK_FABRIC ? HotkeyEvent::ASK_START
                                               : HotkeyEvent::START;
}

HotkeyEvent HotkeyManager::stop_event(TriggerTarget target) {
    return target == TriggerTarget::ASK_FABRIC ? HotkeyEvent::ASK_STOP
                                               : HotkeyEvent::STOP;
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
    std::lock_guard<std::mutex> lock(mutex_);

    pressed_modifiers_.insert(modifier);
    spdlog::debug("Mod down: {}, pressed: {}", modifier, pressed_modifiers_.size());

    // Check for modifier-only trigger
    auto matched = check_any_trigger();
    if (matched && matched->combo.is_modifier_only) {
        spdlog::debug("Modifier-only trigger combo detected");
        if (config_.mode == "toggle" && trigger_pressed_) {
            const auto target = active_trigger_ ? active_trigger_->target : matched->target;
            trigger_pressed_ = false;
            active_trigger_.reset();
            send_event(stop_event(target));
        } else if (!trigger_pressed_) {
            trigger_pressed_ = true;
            active_trigger_ = *matched;
            send_event(start_event(matched->target));
        }
    }
}

void HotkeyManager::on_modifier_release(const std::string& modifier) {
    if (!running_.load()) return;
    std::lock_guard<std::mutex> lock(mutex_);

    pressed_modifiers_.erase(modifier);
    spdlog::debug("Mod up: {}", modifier);

    // For push-to-talk with modifier-only combos
    if (active_trigger_ && active_trigger_->combo.is_modifier_only &&
        config_.mode == "push_to_talk" && trigger_pressed_ &&
        active_trigger_->combo.modifiers.count(modifier) > 0) {
        spdlog::debug("Modifier-only trigger released, stopping");
        auto target = active_trigger_->target;
        trigger_pressed_ = false;
        active_trigger_.reset();
        send_event(stop_event(target));
    }
}

void HotkeyManager::on_key_press(const std::string& key_name) {
    if (!running_.load()) return;
    std::lock_guard<std::mutex> lock(mutex_);

    spdlog::debug("Key pressed: {}", key_name);

    // Check for Escape to cancel
    if (config_.escape_to_cancel && (key_name == "esc" || key_name == "escape")) {
        spdlog::debug("Escape pressed, cancelling");
        trigger_pressed_ = false;
        active_trigger_.reset();
        send_event(HotkeyEvent::CANCEL);
        return;
    }

    // Check cancel combo
    if (check_any_cancel(key_name)) {
        spdlog::debug("Cancel hotkey pressed");
        trigger_pressed_ = false;
        active_trigger_.reset();
        send_event(HotkeyEvent::CANCEL);
        return;
    }

    // Check trigger combo (non-modifier-only)
    auto matched = check_any_trigger(key_name);
    if (matched && !matched->combo.is_modifier_only) {
        if (config_.mode == "toggle") {
            if (trigger_pressed_) {
                auto target = active_trigger_ ? active_trigger_->target : matched->target;
                trigger_pressed_ = false;
                active_trigger_.reset();
                send_event(stop_event(target));
            } else {
                trigger_pressed_ = true;
                active_trigger_ = *matched;
                send_event(start_event(matched->target));
            }
        } else {
            // Push-to-talk: start on press
            if (!trigger_pressed_) {
                trigger_pressed_ = true;
                active_trigger_ = *matched;
                send_event(start_event(matched->target));
            }
        }
    }
}

void HotkeyManager::on_key_release(const std::string& key_name) {
    if (!running_.load()) return;
    std::lock_guard<std::mutex> lock(mutex_);

    // Push-to-talk: stop on release for non-modifier-only combos
    if (active_trigger_ && !active_trigger_->combo.is_modifier_only &&
        config_.mode == "push_to_talk" && trigger_pressed_) {
        if (check_combo(active_trigger_->combo, key_name)) {
            auto target = active_trigger_->target;
            trigger_pressed_ = false;
            active_trigger_.reset();
            send_event(stop_event(target));
        }
    }
}

} // namespace autowhisper

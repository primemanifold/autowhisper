#pragma once

#include "config/config.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace autowhisper {

enum class HotkeyEvent {
    START,
    STOP,
    CANCEL,
};

struct KeyCombo {
    std::set<std::string> modifiers;
    std::string key;  // Empty if modifier-only combo
    bool is_modifier_only = false;

    static KeyCombo parse(const std::string& combo_str);
};

class HotkeyManager {
public:
    using EventCallback = std::function<void(HotkeyEvent)>;

    HotkeyManager(const HotkeyConfig& config, EventCallback callback);
    ~HotkeyManager();

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    void start();
    void stop();
    void signal_stop();  // Non-blocking: signal thread to exit without joining

private:
    HotkeyConfig config_;
    EventCallback callback_;
    std::vector<KeyCombo> trigger_combos_;
    std::vector<KeyCombo> cancel_combos_;
    std::set<std::string> pressed_modifiers_;
    bool trigger_pressed_ = false;
    std::optional<KeyCombo> active_trigger_;
    std::atomic<bool> running_{false};

    // Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void send_event(HotkeyEvent event);
    std::optional<KeyCombo> check_any_trigger(const std::string& key_name = "") const;
    bool check_any_cancel(const std::string& key_name) const;
    bool check_combo(const KeyCombo& combo, const std::string& key_name) const;

    // Called by platform impl
    void on_modifier_press(const std::string& modifier);
    void on_modifier_release(const std::string& modifier);
    void on_key_press(const std::string& key_name);
    void on_key_release(const std::string& key_name);

    friend struct Impl;
};

} // namespace autowhisper

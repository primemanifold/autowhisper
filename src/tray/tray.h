#pragma once

#include "config/config.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace autowhisper {

enum class TrayState {
    IDLE,
    RECORDING,
    PROCESSING,
    ERROR,
};

class TrayManager {
public:
    using QuitCallback = std::function<void()>;
    using SettingsOpenCallback = std::function<void()>;
    using SettingsCloseCallback = std::function<void()>;

    TrayManager(bool enabled,
                QuitCallback on_quit,
                SettingsOpenCallback on_settings_open,
                SettingsCloseCallback on_settings_close,
                const std::string& config_path = "");
    ~TrayManager();

    TrayManager(const TrayManager&) = delete;
    TrayManager& operator=(const TrayManager&) = delete;

    void start();
    void stop();
    void set_state(TrayState state);
    void set_input_device(const std::string& name);
    void set_output_device(const std::string& name);
    void set_hotkey(const std::vector<std::string>& hotkeys);
    void set_cancel_hotkey(const std::vector<std::string>& hotkeys);
    void set_config(const Config& config);

    bool enabled() const { return enabled_; }
    TrayState state() const { return state_; }

private:
    bool enabled_;
    TrayState state_ = TrayState::IDLE;
    QuitCallback on_quit_;
    SettingsOpenCallback on_settings_open_;
    SettingsCloseCallback on_settings_close_;
    std::string config_path_;
    Config config_;

    std::string input_device_ = "Default";
    std::string output_device_ = "Default";
    std::vector<std::string> trigger_hotkeys_ = {"shift+super"};
    std::vector<std::string> cancel_hotkeys_ = {"esc"};

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Helper functions for display
std::string display_key(const std::string& keyname);
std::string display_hotkey(const std::string& hotkey);
std::string format_hotkeys(const std::vector<std::string>& hotkeys);

} // namespace autowhisper

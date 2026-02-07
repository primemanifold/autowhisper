// macOS tray implementation placeholder
#include "tray/tray.h"
#include <spdlog/spdlog.h>

namespace autowhisper {

struct TrayManager::Impl {};

TrayManager::TrayManager(bool enabled, QuitCallback on_quit,
                          SettingsOpenCallback on_settings_open,
                          SettingsCloseCallback on_settings_close,
                          const std::string& config_path)
    : enabled_(false), on_quit_(std::move(on_quit)),
      on_settings_open_(std::move(on_settings_open)),
      on_settings_close_(std::move(on_settings_close)),
      config_path_(config_path), impl_(std::make_unique<Impl>()) {
    if (enabled) spdlog::warn("macOS tray not yet implemented");
}

TrayManager::~TrayManager() = default;
void TrayManager::start() {}
void TrayManager::stop() {}
void TrayManager::set_state(TrayState) {}
void TrayManager::set_input_device(const std::string&) {}
void TrayManager::set_output_device(const std::string&) {}
void TrayManager::set_hotkey(const std::vector<std::string>&) {}
void TrayManager::set_cancel_hotkey(const std::vector<std::string>&) {}
void TrayManager::set_config(const Config&) {}

} // namespace autowhisper

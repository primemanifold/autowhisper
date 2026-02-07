// macOS hotkey implementation placeholder
// Will use CGEventTap for global hotkey monitoring

#include "hotkey/hotkey.h"
#include <spdlog/spdlog.h>

namespace autowhisper {

struct HotkeyManager::Impl {
    HotkeyManager* manager = nullptr;
};

HotkeyManager::HotkeyManager(const HotkeyConfig& config, EventCallback callback)
    : config_(config), callback_(std::move(callback)), impl_(std::make_unique<Impl>()) {
    impl_->manager = this;
    for (const auto& t : config.trigger) trigger_combos_.push_back(KeyCombo::parse(t));
    for (const auto& c : config.cancel) cancel_combos_.push_back(KeyCombo::parse(c));
}

HotkeyManager::~HotkeyManager() { stop(); }
void HotkeyManager::start() { spdlog::warn("macOS hotkey not yet implemented"); }
void HotkeyManager::stop() { running_.store(false); }

} // namespace autowhisper

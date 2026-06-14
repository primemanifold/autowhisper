#pragma once

#include "config/config.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace autowhisper::settings {

nlohmann::json get_config_json(const std::string& path);

nlohmann::json config_to_json(const Config& c);

Config json_to_config(const nlohmann::json& j);

struct ValidationResult {
    bool ok() const { return errors.empty(); }
    std::vector<std::string> errors;
    std::vector<ValidationIssue> issues;
};

ValidationResult validate_json(const nlohmann::json& j);

nlohmann::json issue_to_json(const ValidationIssue& issue);

void save_config_json(const std::string& path, const nlohmann::json& j);

nlohmann::json defaults_json();

// The model catalog with live on-disk availability. This is the single
// source of truth for "is this model ready?" — the UI must derive both the
// download prompt and the ready badge from `downloaded`, never from two
// independent flags (the macOS bug where both showed at once).
nlohmann::json models_json();

// Live OS permission status. On macOS this reports the three TCC grants
// (Microphone, Input Monitoring, Accessibility) so a silently-blocked
// push-to-talk becomes visible; elsewhere it reports applicable=false.
nlohmann::json permissions_json();

}  // namespace autowhisper::settings

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
};

ValidationResult validate_json(const nlohmann::json& j);

void save_config_json(const std::string& path, const nlohmann::json& j);

nlohmann::json defaults_json();

}  // namespace autowhisper::settings

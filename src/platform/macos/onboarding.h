#pragma once

#include <string>

namespace autowhisper {

bool aw_macos_is_app_bundle_launch();
void aw_macos_prompt_required_permissions();
bool aw_macos_launch_setup_helper(const std::string& config_path,
                                  const std::string& setup_error);

} // namespace autowhisper

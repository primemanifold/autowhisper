#pragma once

#include <CLI/CLI.hpp>
#include <string>

namespace autowhisper {

void setup_cli(CLI::App& app);

// Subcommand handlers
int cmd_start();
int cmd_stop();
int cmd_restart();
int cmd_status();
int cmd_logs(bool follow, int lines);
int cmd_run(const std::string& config_path, const std::string& device,
            const std::string& model, bool verbose);
int cmd_config_show();
int cmd_config_edit();
int cmd_config_set(const std::string& key, const std::string& value);
int cmd_config_path();
int cmd_doctor(bool fix);
int cmd_model_list();
int cmd_model_download(const std::string& name);

} // namespace autowhisper

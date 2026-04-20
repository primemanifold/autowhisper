#include "cli/cli.h"
#include "config/config.h"

#include <iostream>

namespace autowhisper {

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string path = config_path_opt.empty()
                               ? get_user_config_path()
                               : config_path_opt;
    std::cout << "Settings UI placeholder. Config path: " << path << "\n";
    return 0;
}

}  // namespace autowhisper

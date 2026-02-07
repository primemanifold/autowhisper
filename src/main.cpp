#include "cli/cli.h"
#include "config/config.h"
#include "daemon/daemon.h"
#include "util/logging.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <iostream>

int main(int argc, char** argv) {
    CLI::App app{"AutoWhisper - GPU-accelerated voice-to-text"};
    app.set_version_flag("-V,--version", AUTOWHISPER_VERSION);

    autowhisper::setup_cli(app);

    // If no subcommand given, default to showing help
    app.require_subcommand(0, 1);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // If no subcommand was provided, show help
    if (app.get_subcommands().empty()) {
        std::cout << app.help() << "\n";
    }

    return 0;
}

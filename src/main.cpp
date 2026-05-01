#include "cli/cli.h"
#include "config/config.h"
#include "daemon/daemon.h"
#include "util/logging.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

bool launched_from_macos_app_bundle(const char* executable_path) {
#if defined(__APPLE__)
    if (executable_path == nullptr) return false;
    const std::string path(executable_path);
    return path.find(".app/Contents/MacOS/") != std::string::npos;
#else
    (void)executable_path;
    return false;
#endif
}

bool has_only_launchservices_args(int argc, char** argv) {
#if defined(__APPLE__)
    if (argc <= 1) return true;
    if (argv == nullptr) return false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) return false;
        const std::string arg(argv[i]);
        if (arg.rfind("-psn_", 0) != 0) return false;
    }
    return true;
#else
    (void)argc;
    (void)argv;
    return false;
#endif
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> app_launch_args;
    std::vector<char*> app_launch_argv;
#if defined(__APPLE__)
    if (argv != nullptr && argc >= 1 && launched_from_macos_app_bundle(argv[0]) &&
        has_only_launchservices_args(argc, argv)) {
        app_launch_args = {argv[0], "run"};
        app_launch_argv.reserve(app_launch_args.size() + 1);
        for (auto& arg : app_launch_args) {
            app_launch_argv.push_back(arg.data());
        }
        argc = static_cast<int>(app_launch_args.size());
        app_launch_argv.push_back(nullptr);
        argv = app_launch_argv.data();
    }
#endif

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

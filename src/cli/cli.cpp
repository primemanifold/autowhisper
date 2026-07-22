#include "cli/cli.h"
#include "config/config.h"
#include "daemon/daemon.h"
#include "doctor/doctor.h"
#include "models/models.h"
#include "protocol/local_protocol.h"
#include "runtime/autowhisper_runtime.h"
#include "runtime/transcription.h"
#include "service/service.h"
#include "util/logging.h"
#include "util/subprocess.h"
#if defined(__APPLE__)
#include "platform/macos/onboarding.h"
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <toml++/toml.hpp>

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace autowhisper {

static std::string get_config_path_cli() {
    std::string user_config = get_user_config_path();
    if (fs::exists(user_config)) return user_config;
    return find_config_file();
}

static Config load_runtime_config(const std::string& config_path,
                                  const std::string& device,
                                  const std::string& model) {
    const std::string path = config_path.empty() ? find_config_file() : config_path;
    Config config = Config::load(path);
    if (!device.empty()) config.model.device = device;
    if (!model.empty()) config.model.size = model;
    return config;
}

static void setup_machine_logging(bool verbose) {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("autowhisper-machine", sink);
    logger->set_level(verbose ? spdlog::level::debug : spdlog::level::warn);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::set_default_logger(std::move(logger));
}

static std::string make_cli_request_id() {
    const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return "cli-" + std::to_string(ticks);
}

void setup_cli(CLI::App& app) {
    // --- Service commands ---
    auto* start_cmd = app.add_subcommand("start", "Start the AutoWhisper service");
    start_cmd->callback([]() { std::exit(cmd_start()); });

    auto* stop_cmd = app.add_subcommand("stop", "Stop the AutoWhisper service");
    stop_cmd->callback([]() { std::exit(cmd_stop()); });

    auto* restart_cmd = app.add_subcommand("restart", "Restart the AutoWhisper service");
    restart_cmd->callback([]() { std::exit(cmd_restart()); });

    auto* status_cmd = app.add_subcommand("status", "Show AutoWhisper service status");
    status_cmd->callback([]() { std::exit(cmd_status()); });

    // Logs
    static bool follow = false;
    static int lines = 50;
    auto* logs_cmd = app.add_subcommand("logs", "Show AutoWhisper service logs");
    logs_cmd->add_flag("-f,--follow", follow, "Follow log output");
    logs_cmd->add_option("-n,--lines", lines, "Number of lines to show")->default_val(50);
    logs_cmd->callback([]() { std::exit(cmd_logs(follow, lines)); });

    // Run (foreground)
    static std::string run_config;
    static std::string run_device;
    static std::string run_model;
    static bool run_verbose = false;
    auto* run_cmd = app.add_subcommand("run", "Run AutoWhisper in foreground (for testing)");
    run_cmd->add_option("-c,--config", run_config, "Config file path");
    run_cmd->add_option("--device", run_device, "Inference device")->check(CLI::IsMember({"cuda", "cpu", "auto"}));
    run_cmd->add_option("--model", run_model, "Model name");
    run_cmd->add_flag("-v,--verbose", run_verbose, "Verbose logging");
    run_cmd->callback([]() { std::exit(cmd_run(run_config, run_device, run_model, run_verbose)); });

    // Reusable local runtime. stdout is reserved for protocol/data output.
    static std::string serve_config;
    static std::string serve_device;
    static std::string serve_model;
    static bool serve_stdio = false;
    static bool serve_verbose = false;
    auto* serve_cmd = app.add_subcommand("serve", "Run the reusable local transcription service");
    serve_cmd->add_option("-c,--config", serve_config, "Config file path");
    serve_cmd->add_option("--device", serve_device, "Inference device")
        ->check(CLI::IsMember({"cuda", "cpu", "auto"}));
    serve_cmd->add_option("--model", serve_model, "Model to keep loaded");
    serve_cmd->add_flag("--stdio", serve_stdio, "Use newline-delimited JSON over stdin/stdout")
        ->required();
    serve_cmd->add_flag("-v,--verbose", serve_verbose, "Verbose logs on stderr");
    serve_cmd->callback([]() {
        std::exit(cmd_serve(serve_config, serve_device, serve_model, serve_stdio, serve_verbose));
    });

    static std::string transcribe_input;
    static std::string transcribe_config;
    static std::string transcribe_device;
    static std::string transcribe_model;
    static std::string transcribe_language;
    static bool transcribe_json = false;
    static bool transcribe_verbose = false;
    auto* transcribe_cmd = app.add_subcommand("transcribe", "Transcribe one local audio file");
    transcribe_cmd->add_option("input", transcribe_input, "Audio file path")->required();
    transcribe_cmd->add_option("-c,--config", transcribe_config, "Config file path");
    transcribe_cmd->add_option("--device", transcribe_device, "Inference device")
        ->check(CLI::IsMember({"cuda", "cpu", "auto"}));
    transcribe_cmd->add_option("--model", transcribe_model, "Model name");
    transcribe_cmd->add_option("--language", transcribe_language, "Language hint or auto");
    transcribe_cmd->add_flag("--json", transcribe_json, "Emit TranscriptionResult v1 JSON");
    transcribe_cmd->add_flag("-v,--verbose", transcribe_verbose, "Verbose logs on stderr");
    transcribe_cmd->callback([]() {
        std::exit(cmd_transcribe(
            transcribe_input,
            transcribe_config,
            transcribe_device,
            transcribe_model,
            transcribe_language,
            transcribe_json,
            transcribe_verbose
        ));
    });

    // --- Config commands ---
    auto* config_cmd = app.add_subcommand("config", "View and edit configuration");
    config_cmd->callback([]() { std::exit(cmd_config_edit()); });

    auto* config_show = config_cmd->add_subcommand("show", "Show current configuration");
    config_show->callback([]() { std::exit(cmd_config_show()); });

    auto* config_edit = config_cmd->add_subcommand("edit", "Open configuration in editor");
    config_edit->callback([]() { std::exit(cmd_config_edit()); });

    static std::string set_key, set_value;
    auto* config_set = config_cmd->add_subcommand("set", "Set a configuration value");
    config_set->add_option("key", set_key, "Key (e.g., model.size)")->required();
    config_set->add_option("value", set_value, "Value")->required();
    config_set->callback([]() { std::exit(cmd_config_set(set_key, set_value)); });

    auto* config_path = config_cmd->add_subcommand("path", "Show config file path");
    config_path->callback([]() { std::exit(cmd_config_path()); });

    static std::string ui_config;
    static bool ui_no_browser = false;
    auto* config_ui = config_cmd->add_subcommand("ui", "Open settings UI in browser");
    config_ui->add_option("-c,--config", ui_config, "Config file path");
    config_ui->add_flag("--no-browser", ui_no_browser, "Do not open a browser window");
    config_ui->callback([]() {
        std::exit(cmd_config_ui(ui_config, !ui_no_browser));
    });

    // --- Doctor ---
    static bool doctor_fix = false;
    auto* avatar_cmd = app.add_subcommand("avatar", "Companion avatar tools");
    auto* avatar_demo = avatar_cmd->add_subcommand(
        "demo", "Show the companion and cycle through its states (smoke test)");
    auto* demo_seconds = new int(14);
    auto* demo_character = new std::string("echo");
    avatar_demo->add_option("--seconds", *demo_seconds, "How long to run");
    avatar_demo->add_option("--character", *demo_character, "echo|hermes|mnemosyne|kalliope|morpheus");
    avatar_demo->callback([demo_seconds, demo_character]() {
        exit(cmd_avatar_demo(*demo_character, *demo_seconds));
    });

    auto* doctor_cmd = app.add_subcommand("doctor", "Diagnose system configuration");
    doctor_cmd->add_flag("--fix", doctor_fix, "Attempt to fix issues");
    doctor_cmd->callback([]() { std::exit(cmd_doctor(doctor_fix)); });

    // --- Model commands ---
    auto* model_cmd = app.add_subcommand("model", "Manage Whisper models");
    model_cmd->require_subcommand();

    auto* model_list = model_cmd->add_subcommand("list", "List available models");
    model_list->callback([]() { std::exit(cmd_model_list()); });

    static std::string dl_name;
    auto* model_dl = model_cmd->add_subcommand("download", "Download a Whisper model");
    model_dl->add_option("name", dl_name, "Model name")->required();
    model_dl->callback([]() { std::exit(cmd_model_download(dl_name)); });
}

// === Service commands ===

int cmd_start() {
    auto svc = make_service_manager();
    auto result = svc->start();
    if (result.exit_code == 0) {
        std::cout << "\033[32mAutoWhisper started\033[0m\n";
        return 0;
    }
    std::cerr << "\033[31mFailed to start: " << result.stderr_str << "\033[0m\n";
    return 1;
}

int cmd_stop() {
    auto svc = make_service_manager();
    auto result = svc->stop();
    if (result.exit_code == 0) {
        std::cout << "\033[32mAutoWhisper stopped\033[0m\n";
        return 0;
    }
    std::cerr << "\033[31mFailed to stop: " << result.stderr_str << "\033[0m\n";
    return 1;
}

int cmd_restart() {
    auto svc = make_service_manager();
    auto result = svc->restart();
    if (result.exit_code == 0) {
        std::cout << "\033[32mAutoWhisper restarted\033[0m\n";
        return 0;
    }
    std::cerr << "\033[31mFailed to restart: " << result.stderr_str << "\033[0m\n";
    return 1;
}

int cmd_status() {
    auto svc = make_service_manager();
    auto result = svc->status();
    std::cout << result.stdout_str;
    if (!result.stderr_str.empty()) std::cerr << result.stderr_str;
    return result.exit_code;
}

int cmd_logs(bool follow, int lines) {
    auto svc = make_service_manager();
    return svc->tail_logs(follow, lines);
}

int cmd_run(const std::string& config_path, const std::string& device,
            const std::string& model, bool verbose) {
    const bool app_bundle_launch =
#if defined(__APPLE__)
        aw_macos_is_app_bundle_launch();
#else
        false;
#endif
    std::string path;
    try {
        path = config_path.empty()
            ? (app_bundle_launch ? ensure_user_config_file() : find_config_file())
            : config_path;
        std::cout << "Loading configuration from " << path << "\n";
        Config config = Config::load(path);

#if defined(__APPLE__)
        if (app_bundle_launch) {
            aw_macos_prompt_required_permissions();
        }
#endif

        // Apply overrides
        if (verbose) config.daemon.log_level = "debug";
        if (!device.empty()) config.model.device = device;
        if (!model.empty()) config.model.size = model;

        setup_logging(config.daemon.log_level,
                      config.daemon.log_file.value_or(""));

        spdlog::info("AutoWhisper v" AUTOWHISPER_VERSION " starting");
        spdlog::info("Model: {} on {}", config.model.size, config.model.device);

        AutoWhisperDaemon daemon(std::move(config), path);
        daemon.initialize();
        daemon.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
#if defined(__APPLE__)
        if (app_bundle_launch) {
            if (path.empty()) {
                try {
                    path = ensure_user_config_file();
                } catch (...) {
                    path.clear();
                }
            }
            aw_macos_prompt_required_permissions();
            if (aw_macos_launch_setup_helper(path, e.what())) {
                return 0;
            }
        }
#endif
        return 1;
    }
}

int cmd_serve(const std::string& config_path, const std::string& device,
              const std::string& model, bool stdio, bool verbose) {
    if (!stdio) {
        std::cerr << "Error: serve currently requires --stdio\n";
        return 2;
    }
    setup_machine_logging(verbose);
    try {
        Config config = load_runtime_config(config_path, device, model);
        AutoWhisperRuntime runtime(config.model);
        runtime.load();
        ProtocolHandler handler(runtime);
        StdioServer server(handler);
        return server.run(std::cin, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "AutoWhisper service failed: " << error.what() << "\n";
        return 1;
    }
}

int cmd_transcribe(const std::string& input_path, const std::string& config_path,
                   const std::string& device, const std::string& model,
                   const std::string& language, bool json_output, bool verbose) {
    setup_machine_logging(verbose);
    try {
        Config config = load_runtime_config(config_path, device, model);
        AutoWhisperRuntime runtime(config.model);
        runtime.load();
        TranscriptionOptions options;
        options.request_id = make_cli_request_id();
        options.language = language;
        options.model = model;
        TranscriptionResult result = runtime.transcribe_file(input_path, options);
        if (json_output) {
            std::cout << nlohmann::json(result).dump() << "\n";
        } else if (result.status == TranscriptionStatus::completed) {
            std::cout << result.text << "\n";
        } else if (result.status == TranscriptionStatus::no_speech) {
            std::cout << "\n";
        } else if (result.error.has_value()) {
            std::cerr << "Transcription failed: " << result.error->message << "\n";
        }
        return result.status == TranscriptionStatus::failed ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Transcription failed: " << error.what() << "\n";
        return 1;
    }
}

// === Config commands ===

int cmd_config_show() {
    try {
        std::string path = get_config_path_cli();
        std::cout << "Config file: " << path << "\n\n";
        std::ifstream ifs(path);
        std::cout << ifs.rdbuf();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[31mNo config file found\033[0m\n";
        return 1;
    }
}

int cmd_config_edit() {
    try {
        std::string path = get_config_path_cli();
        const char* editor = std::getenv("EDITOR");
        if (!editor) editor = "nano";
        return run_passthrough({editor, path});
    } catch (...) {
        // Create user config dir
        std::string user_config = get_user_config_path();
        fs::create_directories(fs::path(user_config).parent_path());

        if (fs::exists("/etc/autowhisper/config.toml")) {
            fs::copy_file("/etc/autowhisper/config.toml", user_config);
        } else {
            std::cerr << "\033[31mNo config file found to edit\033[0m\n";
            return 1;
        }

        const char* editor = std::getenv("EDITOR");
        if (!editor) editor = "nano";
        return run_passthrough({editor, user_config});
    }
}

int cmd_config_set(const std::string& key, const std::string& value) {
    try {
        std::string path = get_config_path_cli();

        // Parse section.key
        auto dot = key.find('.');
        if (dot == std::string::npos) {
            std::cerr << "\033[31mKey must be in format: section.key\033[0m\n";
            return 1;
        }

        // Load, modify, save
        Config config = Config::load(path);
        // We need to work with the raw TOML for this
        auto tbl = toml::parse_file(path);

        std::string section = key.substr(0, dot);
        std::string option = key.substr(dot + 1);

        if (!tbl[section].is_table()) {
            std::cerr << "\033[31mUnknown section: " << section << "\033[0m\n";
            return 1;
        }

        auto& sect = *tbl[section].as_table();

        // Type detection
        if (value == "true" || value == "false") {
            sect.insert_or_assign(option, value == "true");
        } else {
            try {
                size_t pos;
                int ival = std::stoi(value, &pos);
                if (pos == value.size()) {
                    sect.insert_or_assign(option, static_cast<int64_t>(ival));
                } else {
                    double dval = std::stod(value, &pos);
                    if (pos == value.size()) {
                        sect.insert_or_assign(option, dval);
                    } else {
                        sect.insert_or_assign(option, value);
                    }
                }
            } catch (...) {
                sect.insert_or_assign(option, value);
            }
        }

        std::ofstream ofs(path);
        ofs << tbl;

        std::cout << "\033[32mSet " << key << " = " << value << "\033[0m\n";
        std::cout << "Restart service for changes to take effect: autowhisper restart\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[31m" << e.what() << "\033[0m\n";
        return 1;
    }
}

int cmd_config_path() {
    try {
        std::cout << get_config_path_cli() << "\n";
        return 0;
    } catch (...) {
        std::cerr << "\033[31mNo config file found\033[0m\n";
        return 1;
    }
}

// === Doctor ===
int cmd_doctor(bool fix) {
    return run_diagnostics(fix);
}

// === Model commands ===
int cmd_model_list() {
    list_models();
    return 0;
}

int cmd_model_download(const std::string& name) {
    return download_model(name);
}

} // namespace autowhisper

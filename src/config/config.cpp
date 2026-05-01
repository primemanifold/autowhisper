#include "config/config.h"

#include "config/schema.h"

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace autowhisper {

namespace {

template <typename T>
T get_or(const toml::table& tbl, const std::string& key, const T& def) {
    auto node = tbl[key];
    if (node) {
        if constexpr (std::is_same_v<T, std::string>) {
            if (auto val = node.value<std::string>()) return *val;
        } else if constexpr (std::is_same_v<T, int>) {
            if (auto val = node.value<int64_t>()) return static_cast<int>(*val);
        } else if constexpr (std::is_same_v<T, float>) {
            if (auto val = node.value<double>()) return static_cast<float>(*val);
        } else if constexpr (std::is_same_v<T, bool>) {
            if (auto val = node.value<bool>()) return *val;
        }
    }
    return def;
}

std::optional<std::string> get_optional_string(const toml::table& tbl, const std::string& key) {
    auto node = tbl[key];
    if (node) {
        if (auto val = node.value<std::string>()) {
            if (*val == "default" || val->empty()) return std::nullopt;
            return *val;
        }
    }
    return std::nullopt;
}

std::vector<std::string> get_string_array(const toml::table& tbl, const std::string& key,
                                           const std::vector<std::string>& def) {
    auto node = tbl[key];
    if (!node) return def;

    // Handle single string (backwards compat)
    if (auto val = node.value<std::string>()) {
        return {*val};
    }

    // Handle array
    if (auto arr = node.as_array()) {
        std::vector<std::string> result;
        for (const auto& elem : *arr) {
            if (auto val = elem.value<std::string>()) {
                result.push_back(*val);
            }
        }
        if (!result.empty()) return result;
    }

    return def;
}

ValidationIssue make_issue(ValidationSeverity severity, std::string path,
                           std::string code, std::string message) {
    return ValidationIssue{severity, std::move(path), std::move(code), std::move(message)};
}

void add_error(std::vector<ValidationIssue>& issues, std::string path,
               std::string code, std::string message) {
    issues.push_back(make_issue(ValidationSeverity::Error, std::move(path), std::move(code), std::move(message)));
}

bool is_allowed_enum(std::string_view section, std::string_view key, const std::string& value) {
    const auto* d = schema::find(section, key);
    if (!d || d->type != schema::Type::Enum) return true;
    return std::any_of(d->enum_values.begin(), d->enum_values.end(), [&](auto allowed) {
        return value == allowed;
    });
}

std::string find_bundled_config_file() {
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) return {};

    std::string executable_path(size, '\0');
    if (_NSGetExecutablePath(executable_path.data(), &size) != 0) return {};
    executable_path.resize(std::char_traits<char>::length(executable_path.c_str()));

    std::error_code ec;
    const fs::path canonical_executable = fs::weakly_canonical(executable_path, ec);
    const fs::path path = ec ? fs::path(executable_path) : canonical_executable;
    const fs::path resources_config = path.parent_path().parent_path() / "Resources" / "config.toml";
    if (fs::is_regular_file(resources_config)) {
        return resources_config.string();
    }
#endif
    return {};
}

void collect_unknown_toml_issues(const toml::table& tbl, std::vector<ValidationIssue>& issues) {
    for (const auto& [section_key, section_node] : tbl) {
        const std::string section(section_key.str());
        if (section == "schema_version") continue;

        if (!section_node.is_table()) {
            issues.push_back(make_issue(ValidationSeverity::Warning, section, "unknown_key",
                                        "Unknown top-level config key: " + section));
            continue;
        }

        if (!schema::is_known_section(section)) {
            issues.push_back(make_issue(ValidationSeverity::Warning, section, "unknown_section",
                                        "Unknown config section: " + section));
            continue;
        }

        const auto* section_table = section_node.as_table();
        for (const auto& item : *section_table) {
            const std::string key_s(item.first.str());
            // Legacy key translated to output.ending_action below; do not warn for it.
            if (section == "output" && key_s == "append_newline") continue;
            if (!schema::is_known_key(section, key_s)) {
                issues.push_back(make_issue(ValidationSeverity::Warning, section + "." + key_s,
                                            "unknown_key", "Unknown config key: " + section + "." + key_s));
            }
        }
    }
}

} // anonymous namespace

Config Config::load(const std::string& path) {
    auto result = load_with_diagnostics(path);
    for (const auto& issue : result.issues) {
        if (issue.severity == ValidationSeverity::Warning) {
            spdlog::warn("{}", issue.message);
        }
    }

    auto error = std::find_if(result.issues.begin(), result.issues.end(), [](const ValidationIssue& issue) {
        return issue.severity == ValidationSeverity::Error;
    });
    if (error != result.issues.end()) {
        throw std::runtime_error(error->message);
    }

    if (result.config.audio.sample_rate != 16000) {
        spdlog::warn("Sample rate {} is not Whisper's native 16kHz", result.config.audio.sample_rate);
    }
    return result.config;
}

ConfigLoadResult Config::load_with_diagnostics(const std::string& path) {
    if (!fs::exists(path)) {
        throw std::runtime_error("Config file not found: " + path);
    }

    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& err) {
        throw std::runtime_error(std::string("TOML parse error: ") + err.what());
    }

    Config config;
    std::vector<ValidationIssue> issues;
    collect_unknown_toml_issues(tbl, issues);

    // [model]
    if (auto model = tbl["model"].as_table()) {
        config.model.size = get_or(*model, "size", config.model.size);
        config.model.device = get_or(*model, "device", config.model.device);
        config.model.compute_type = get_or(*model, "compute_type", config.model.compute_type);
        config.model.beam_size = get_or(*model, "beam_size", config.model.beam_size);
        config.model.language = get_or(*model, "language", config.model.language);
        config.model.num_threads = get_or(*model, "num_threads", config.model.num_threads);
    }

    // [audio]
    if (auto audio = tbl["audio"].as_table()) {
        config.audio.sample_rate = get_or(*audio, "sample_rate", config.audio.sample_rate);
        config.audio.channels = get_or(*audio, "channels", config.audio.channels);
        config.audio.buffer_size = get_or(*audio, "buffer_size", config.audio.buffer_size);
        config.audio.device = get_optional_string(*audio, "device");
        config.audio.output_device = get_optional_string(*audio, "output_device");
        config.audio.vad_enabled = get_or(*audio, "vad_enabled", config.audio.vad_enabled);
        config.audio.vad_threshold = get_or(*audio, "vad_threshold", config.audio.vad_threshold);
        config.audio.silence_duration = get_or(*audio, "silence_duration", config.audio.silence_duration);
        config.audio.max_duration = get_or(*audio, "max_duration", config.audio.max_duration);
        config.audio.mute_other_apps = get_or(*audio, "mute_other_apps", config.audio.mute_other_apps);
    }

    // [hotkeys]
    if (auto hotkeys = tbl["hotkeys"].as_table()) {
        config.hotkeys.mode = get_or(*hotkeys, "mode", config.hotkeys.mode);
        config.hotkeys.trigger = get_string_array(*hotkeys, "trigger", config.hotkeys.trigger);
        config.hotkeys.cancel = get_string_array(*hotkeys, "cancel", config.hotkeys.cancel);
        config.hotkeys.escape_to_cancel = get_or(*hotkeys, "escape_to_cancel", config.hotkeys.escape_to_cancel);
    }

    // [output]
    if (auto output = tbl["output"].as_table()) {
        config.output.method = get_or(*output, "method", config.output.method);
        config.output.auto_paste = get_or(*output, "auto_paste", config.output.auto_paste);
        config.output.paste_delay = get_or(*output, "paste_delay", config.output.paste_delay);
        config.output.also_copy_to_clipboard = get_or(*output, "also_copy_to_clipboard", config.output.also_copy_to_clipboard);
        config.output.lowercase = get_or(*output, "lowercase", config.output.lowercase);

        // Handle ending_action with backwards compat for append_newline
        if (auto ea = output->at_path("ending_action").value<std::string>()) {
            config.output.ending_action = *ea;
        } else if (auto an = output->at_path("append_newline").value<bool>()) {
            config.output.ending_action = *an ? "newline" : "none";
        }
    }

    // [feedback]
    if (auto feedback = tbl["feedback"].as_table()) {
        config.feedback.enabled = get_or(*feedback, "enabled", config.feedback.enabled);
        config.feedback.frequency_start = get_or(*feedback, "frequency_start", config.feedback.frequency_start);
        config.feedback.frequency_stop = get_or(*feedback, "frequency_stop", config.feedback.frequency_stop);
        config.feedback.frequency_error = get_or(*feedback, "frequency_error", config.feedback.frequency_error);
        config.feedback.duration = get_or(*feedback, "duration", config.feedback.duration);
        config.feedback.volume = get_or(*feedback, "volume", config.feedback.volume);
    }

    // [daemon]
    if (auto daemon = tbl["daemon"].as_table()) {
        config.daemon.log_level = get_or(*daemon, "log_level", config.daemon.log_level);
        config.daemon.pid_file = get_or(*daemon, "pid_file", config.daemon.pid_file);
        config.daemon.work_dir = get_or(*daemon, "work_dir", config.daemon.work_dir);
        config.daemon.log_file = get_optional_string(*daemon, "log_file");
    }

    // [tray]
    if (auto tray = tbl["tray"].as_table()) {
        config.tray.enabled = get_or(*tray, "enabled", config.tray.enabled);
    }

    auto validation_issues = config.validate_all();
    issues.insert(issues.end(), validation_issues.begin(), validation_issues.end());
    return ConfigLoadResult{config, std::move(issues)};
}

Config Config::default_config() {
    return Config{};
}

std::vector<ValidationIssue> Config::validate_all() const {
    std::vector<ValidationIssue> issues;

    if (!is_allowed_enum("model", "size", model.size)) {
        add_error(issues, "model.size", "invalid_enum", "Invalid model size: " + model.size);
    }
    if (!is_allowed_enum("model", "device", model.device)) {
        add_error(issues, "model.device", "invalid_enum", "Invalid device: " + model.device);
    }
    if (!is_allowed_enum("model", "compute_type", model.compute_type)) {
        add_error(issues, "model.compute_type", "invalid_enum", "Invalid compute_type: " + model.compute_type);
    }

    if (model.beam_size <= 0 || model.beam_size > 10) {
        add_error(issues, "model.beam_size", "out_of_range", "Invalid beam_size: must be between 1 and 10");
    }
    if (model.num_threads <= 0 || model.num_threads > 256) {
        add_error(issues, "model.num_threads", "out_of_range", "Invalid num_threads: must be between 1 and 256");
    }

    if (audio.sample_rate <= 0) {
        add_error(issues, "audio.sample_rate", "out_of_range", "Invalid sample_rate: must be > 0");
    }
    if (audio.channels <= 0 || audio.channels > 8) {
        add_error(issues, "audio.channels", "out_of_range", "Invalid channels: must be between 1 and 8");
    }
    if (audio.buffer_size <= 0) {
        add_error(issues, "audio.buffer_size", "out_of_range", "Invalid buffer_size: must be > 0");
    }
    if (!std::isfinite(audio.vad_threshold) || audio.vad_threshold < 0.0f || audio.vad_threshold > 1.0f) {
        add_error(issues, "audio.vad_threshold", "out_of_range", "Invalid vad_threshold: must be between 0.0 and 1.0");
    }
    if (!std::isfinite(audio.silence_duration) || audio.silence_duration <= 0.0f) {
        add_error(issues, "audio.silence_duration", "out_of_range", "Invalid silence_duration: must be > 0");
    }
    if (!std::isfinite(audio.max_duration) || audio.max_duration <= 0.0f) {
        add_error(issues, "audio.max_duration", "out_of_range", "Invalid max_duration: must be > 0");
    }

    if (!is_allowed_enum("hotkeys", "mode", hotkeys.mode)) {
        add_error(issues, "hotkeys.mode", "invalid_enum", "Invalid hotkey mode: " + hotkeys.mode);
    }
    if (hotkeys.trigger.empty()) {
        add_error(issues, "hotkeys.trigger", "missing_required", "Invalid hotkeys.trigger: at least one trigger is required");
    }
    for (const auto& key : hotkeys.trigger) {
        if (key.empty()) {
            add_error(issues, "hotkeys.trigger", "empty_entry", "Invalid hotkeys.trigger: empty key entry");
        }
    }
    for (const auto& key : hotkeys.cancel) {
        if (key.empty()) {
            add_error(issues, "hotkeys.cancel", "empty_entry", "Invalid hotkeys.cancel: empty key entry");
        }
    }

    if (!is_allowed_enum("output", "method", output.method)) {
        add_error(issues, "output.method", "invalid_enum", "Invalid output method: " + output.method);
    }
    if (!std::isfinite(output.paste_delay) || output.paste_delay < 0.0f ||
        output.paste_delay > static_cast<float>(std::numeric_limits<int>::max())) {
        add_error(issues, "output.paste_delay", "out_of_range", "Invalid paste_delay: must be >= 0");
    }

    if (!is_allowed_enum("output", "ending_action", output.ending_action)) {
        add_error(issues, "output.ending_action", "invalid_enum", "Invalid ending_action: " + output.ending_action);
    }

    if (!std::isfinite(feedback.volume) || feedback.volume < 0.0f || feedback.volume > 1.0f) {
        add_error(issues, "feedback.volume", "out_of_range", "Invalid volume: must be between 0.0 and 1.0");
    }
    if (feedback.frequency_start <= 0 || feedback.frequency_stop <= 0 ||
        feedback.frequency_error <= 0) {
        add_error(issues, "feedback.frequency", "out_of_range", "Invalid feedback frequencies: must be > 0");
    }
    if (!std::isfinite(feedback.duration) || feedback.duration <= 0.0f) {
        add_error(issues, "feedback.duration", "out_of_range", "Invalid feedback duration: must be > 0");
    }

    if (!is_allowed_enum("daemon", "log_level", daemon.log_level)) {
        add_error(issues, "daemon.log_level", "invalid_enum", "Invalid daemon.log_level: " + daemon.log_level);
    }
    if (daemon.pid_file.empty()) {
        add_error(issues, "daemon.pid_file", "empty_value", "Invalid daemon.pid_file: cannot be empty");
    }
    if (daemon.work_dir.empty()) {
        add_error(issues, "daemon.work_dir", "empty_value", "Invalid daemon.work_dir: cannot be empty");
    }

    return issues;
}

std::vector<ValidationIssue> ConfigValidator::validate(const Config& config) {
    return config.validate_all();
}

void Config::validate() const {
    const auto issues = validate_all();
    auto it = std::find_if(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
        return issue.severity == ValidationSeverity::Error;
    });
    if (it != issues.end()) {
        throw std::runtime_error(it->message);
    }

    if (audio.sample_rate != 16000) {
        spdlog::warn("Sample rate {} is not Whisper's native 16kHz", audio.sample_rate);
    }
}

void Config::save(const std::string& path) const {
    validate();

    toml::table tbl;

    // [model]
    tbl.insert("model", toml::table{
        {"size", model.size},
        {"device", model.device},
        {"compute_type", model.compute_type},
        {"beam_size", static_cast<int64_t>(model.beam_size)},
        {"language", model.language},
        {"num_threads", static_cast<int64_t>(model.num_threads)},
    });

    // [audio]
    toml::table audio_tbl;
    audio_tbl.insert("sample_rate", static_cast<int64_t>(audio.sample_rate));
    audio_tbl.insert("channels", static_cast<int64_t>(audio.channels));
    audio_tbl.insert("buffer_size", static_cast<int64_t>(audio.buffer_size));
    if (audio.device) audio_tbl.insert("device", *audio.device);
    if (audio.output_device) audio_tbl.insert("output_device", *audio.output_device);
    audio_tbl.insert("vad_enabled", audio.vad_enabled);
    audio_tbl.insert("vad_threshold", static_cast<double>(audio.vad_threshold));
    audio_tbl.insert("silence_duration", static_cast<double>(audio.silence_duration));
    audio_tbl.insert("max_duration", static_cast<double>(audio.max_duration));
    audio_tbl.insert("mute_other_apps", audio.mute_other_apps);
    tbl.insert("audio", std::move(audio_tbl));

    // [hotkeys]
    toml::array trigger_arr;
    for (const auto& t : hotkeys.trigger) trigger_arr.push_back(t);
    toml::array cancel_arr;
    for (const auto& c : hotkeys.cancel) cancel_arr.push_back(c);
    tbl.insert("hotkeys", toml::table{
        {"mode", hotkeys.mode},
        {"trigger", std::move(trigger_arr)},
        {"cancel", std::move(cancel_arr)},
        {"escape_to_cancel", hotkeys.escape_to_cancel},
    });

    // [output]
    tbl.insert("output", toml::table{
        {"method", output.method},
        {"also_copy_to_clipboard", output.also_copy_to_clipboard},
        {"auto_paste", output.auto_paste},
        {"paste_delay", static_cast<double>(output.paste_delay)},
        {"ending_action", output.ending_action},
        {"lowercase", output.lowercase},
    });

    // [feedback]
    tbl.insert("feedback", toml::table{
        {"enabled", feedback.enabled},
        {"frequency_start", static_cast<int64_t>(feedback.frequency_start)},
        {"frequency_stop", static_cast<int64_t>(feedback.frequency_stop)},
        {"frequency_error", static_cast<int64_t>(feedback.frequency_error)},
        {"duration", static_cast<double>(feedback.duration)},
        {"volume", static_cast<double>(feedback.volume)},
    });

    // [daemon]
    toml::table daemon_tbl;
    daemon_tbl.insert("log_level", daemon.log_level);
    if (daemon.log_file) daemon_tbl.insert("log_file", *daemon.log_file);
    daemon_tbl.insert("pid_file", daemon.pid_file);
    daemon_tbl.insert("work_dir", daemon.work_dir);
    tbl.insert("daemon", std::move(daemon_tbl));

    // [tray]
    tbl.insert("tray", toml::table{
        {"enabled", tray.enabled},
    });

    // Write to file
    const fs::path out_path(path);
    const fs::path parent = out_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            throw std::runtime_error("Cannot create config directory: " + parent.string());
        }
    }

    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Cannot open config file for writing: " + path);
    }
    ofs << tbl;
}

std::string find_config_file() {
    const std::string bundled_config = find_bundled_config_file();

    // Search hierarchy
    std::vector<std::string> search_paths = {
        "config.toml",
        get_user_config_path(),
        bundled_config,
        "/etc/autowhisper/config.toml",
        "/opt/autowhisper/config.toml",
    };

    for (const auto& path : search_paths) {
        if (fs::is_regular_file(path)) {
            return path;
        }
    }

    throw std::runtime_error(
        "No config file found. Searched: config.toml, ~/.config/autowhisper/config.toml, "
        "/etc/autowhisper/config.toml, /opt/autowhisper/config.toml");
}

std::string get_user_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.config/autowhisper/config.toml";
}

std::string resolve_config_path() {
    std::string user_config = get_user_config_path();
    if (fs::exists(user_config)) return user_config;
    try {
        return find_config_file();
    } catch (...) {
        return user_config;
    }
}

std::string ensure_user_config_file() {
    const std::string user_config = get_user_config_path();
    const fs::path user_config_path(user_config);
    if (fs::is_regular_file(user_config_path)) {
        return user_config;
    }

    const fs::path parent = user_config_path.parent_path();
    if (!parent.empty()) {
        std::error_code mkdir_ec;
        fs::create_directories(parent, mkdir_ec);
        if (mkdir_ec) {
            throw std::runtime_error("Cannot create config directory: " + parent.string());
        }
    }

    std::string source;
    try {
        source = find_config_file();
    } catch (...) {
        source.clear();
    }

    if (!source.empty() && fs::is_regular_file(source) && fs::path(source) != user_config_path) {
        std::error_code copy_ec;
        fs::copy_file(source, user_config_path, fs::copy_options::overwrite_existing, copy_ec);
        if (!copy_ec) {
            return user_config;
        }
        spdlog::warn("Could not copy default config from {} to {}: {}", source, user_config, copy_ec.message());
    }

    Config::default_config().save(user_config);
    return user_config;
}

} // namespace autowhisper

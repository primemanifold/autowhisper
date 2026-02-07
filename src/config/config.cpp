#include "config/config.h"

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

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

} // anonymous namespace

Config Config::load(const std::string& path) {
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
        if (auto lf = daemon->at_path("log_file").value<std::string>()) {
            config.daemon.log_file = *lf;
        }
    }

    // [tray]
    if (auto tray = tbl["tray"].as_table()) {
        config.tray.enabled = get_or(*tray, "enabled", config.tray.enabled);
    }

    config.validate();
    return config;
}

Config Config::default_config() {
    return Config{};
}

void Config::validate() const {
    // Validate model size
    static const std::set<std::string> valid_models = {
        "tiny", "tiny.en", "base", "base.en", "small", "small.en",
        "medium", "medium.en", "large", "large-v1", "large-v2", "large-v3",
        "distil-large-v2", "distil-large-v3", "distil-medium.en", "distil-small.en",
    };
    if (valid_models.find(model.size) == valid_models.end()) {
        throw std::runtime_error("Invalid model size: " + model.size);
    }

    // Validate device
    static const std::set<std::string> valid_devices = {"cuda", "cpu", "auto"};
    if (valid_devices.find(model.device) == valid_devices.end()) {
        throw std::runtime_error("Invalid device: " + model.device);
    }

    // Validate compute type
    static const std::set<std::string> valid_compute = {
        "float16", "float32", "int8", "int8_float16",
        "int8_float32", "int8_bfloat16", "bfloat16",
    };
    if (valid_compute.find(model.compute_type) == valid_compute.end()) {
        throw std::runtime_error("Invalid compute_type: " + model.compute_type);
    }

    if (audio.sample_rate != 16000) {
        spdlog::warn("Sample rate {} is not Whisper's native 16kHz", audio.sample_rate);
    }

    // Validate hotkey mode
    if (hotkeys.mode != "push_to_talk" && hotkeys.mode != "toggle") {
        throw std::runtime_error("Invalid hotkey mode: " + hotkeys.mode);
    }

    // Validate output method
    if (output.method != "inject" && output.method != "clipboard") {
        throw std::runtime_error("Invalid output method: " + output.method);
    }

    // Validate ending action
    if (output.ending_action != "none" && output.ending_action != "newline" &&
        output.ending_action != "return_key") {
        throw std::runtime_error("Invalid ending_action: " + output.ending_action);
    }

    // Validate volume
    if (feedback.volume < 0.0f || feedback.volume > 1.0f) {
        throw std::runtime_error("Invalid volume: must be between 0.0 and 1.0");
    }
}

void Config::save(const std::string& path) const {
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
    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Cannot open config file for writing: " + path);
    }
    ofs << tbl;
}

std::string find_config_file() {
    // Search hierarchy
    std::vector<std::string> search_paths = {
        "config.toml",
        get_user_config_path(),
        "/etc/autowhisper/config.toml",
        "/opt/autowhisper/config.toml",
    };

    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
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

} // namespace autowhisper

#include "settings/handlers.h"
#include "config/config.h"
#include "models/models.h"

#ifdef __APPLE__
#include "platform/macos/onboarding.h"
#endif

#include <nlohmann/json.hpp>

#include <cerrno>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace autowhisper::settings {

namespace {

class JsonFieldError : public std::runtime_error {
public:
    JsonFieldError(std::string path, const std::string& message)
        : std::runtime_error(message), path_(std::move(path)) {}
    const std::string& path() const noexcept { return path_; }
private:
    std::string path_;
};

}  // namespace

nlohmann::json config_to_json(const Config& c) {
    nlohmann::json j;

    j["model"] = {
        {"size", c.model.size},
        {"device", c.model.device},
        {"compute_type", c.model.compute_type},
        {"beam_size", c.model.beam_size},
        {"language", c.model.language},
        {"num_threads", c.model.num_threads},
    };

    j["audio"] = {
        {"sample_rate", c.audio.sample_rate},
        {"channels", c.audio.channels},
        {"buffer_size", c.audio.buffer_size},
        {"device", c.audio.device.value_or("")},
        {"output_device", c.audio.output_device.value_or("")},
        {"vad_enabled", c.audio.vad_enabled},
        {"vad_threshold", shortest_double(c.audio.vad_threshold)},
        {"silence_duration", shortest_double(c.audio.silence_duration)},
        {"max_duration", shortest_double(c.audio.max_duration)},
        {"mute_other_apps", c.audio.mute_other_apps},
    };

    j["hotkeys"] = {
        {"mode", c.hotkeys.mode},
        {"trigger", c.hotkeys.trigger},
        {"ask_trigger", c.hotkeys.ask_trigger},
        {"cancel", c.hotkeys.cancel},
        {"escape_to_cancel", c.hotkeys.escape_to_cancel},
    };

    j["fabric"] = {
        {"enabled", c.fabric.enabled},
        {"executable", c.fabric.executable},
        {"timeout_seconds", c.fabric.timeout_seconds},
    };

    j["output"] = {
        {"method", c.output.method},
        {"auto_paste", c.output.auto_paste},
        {"paste_delay", shortest_double(c.output.paste_delay)},
        {"ending_action", c.output.ending_action},
        {"lowercase", c.output.lowercase},
        {"also_copy_to_clipboard", c.output.also_copy_to_clipboard},
    };

    j["formatting"] = {
        {"remove_fillers", c.formatting.remove_fillers},
        {"spoken_commands", c.formatting.spoken_commands},
        {"dictionary", c.formatting.dictionary},
    };

    j["feedback"] = {
        {"enabled", c.feedback.enabled},
        {"frequency_start", c.feedback.frequency_start},
        {"frequency_stop", c.feedback.frequency_stop},
        {"frequency_error", c.feedback.frequency_error},
        {"duration", shortest_double(c.feedback.duration)},
        {"volume", shortest_double(c.feedback.volume)},
    };

    j["daemon"] = {
        {"log_level", c.daemon.log_level},
        {"log_file", c.daemon.log_file.value_or("")},
        {"pid_file", c.daemon.pid_file},
        {"work_dir", c.daemon.work_dir},
    };

    j["tray"] = {
        {"enabled", c.tray.enabled},
    };

    j["avatar"] = {
        {"enabled", c.avatar.enabled},
        {"character", c.avatar.character},
        {"size", c.avatar.size},
    };

    return j;
}

Config json_to_config(const nlohmann::json& j) {
    Config c = Config::default_config();

    auto get = [&](const char* section, const char* key, auto& dst) {
        if (j.contains(section) && j[section].contains(key)) {
            try {
                dst = j[section][key].get<std::decay_t<decltype(dst)>>();
            } catch (const std::exception& e) {
                throw JsonFieldError(
                    std::string(section) + "." + key,
                    std::string("Invalid ") + section + "." + key + ": " + e.what());
            }
        }
    };

    get("model", "size", c.model.size);
    get("model", "device", c.model.device);
    get("model", "compute_type", c.model.compute_type);
    get("model", "beam_size", c.model.beam_size);
    get("model", "language", c.model.language);
    get("model", "num_threads", c.model.num_threads);

    get("audio", "sample_rate", c.audio.sample_rate);
    get("audio", "channels", c.audio.channels);
    get("audio", "buffer_size", c.audio.buffer_size);
    get("audio", "vad_enabled", c.audio.vad_enabled);
    get("audio", "vad_threshold", c.audio.vad_threshold);
    get("audio", "silence_duration", c.audio.silence_duration);
    get("audio", "max_duration", c.audio.max_duration);
    get("audio", "mute_other_apps", c.audio.mute_other_apps);
    if (j.contains("audio") && j["audio"].contains("device")) {
        auto s = j["audio"]["device"].get<std::string>();
        c.audio.device = s.empty() ? std::nullopt : std::optional<std::string>(s);
    }
    if (j.contains("audio") && j["audio"].contains("output_device")) {
        auto s = j["audio"]["output_device"].get<std::string>();
        c.audio.output_device = s.empty() ? std::nullopt : std::optional<std::string>(s);
    }

    get("hotkeys", "mode", c.hotkeys.mode);
    get("hotkeys", "trigger", c.hotkeys.trigger);
    get("hotkeys", "ask_trigger", c.hotkeys.ask_trigger);
    get("hotkeys", "cancel", c.hotkeys.cancel);
    get("hotkeys", "escape_to_cancel", c.hotkeys.escape_to_cancel);

    get("fabric", "enabled", c.fabric.enabled);
    get("fabric", "executable", c.fabric.executable);
    get("fabric", "timeout_seconds", c.fabric.timeout_seconds);

    get("output", "method", c.output.method);
    get("output", "auto_paste", c.output.auto_paste);
    get("output", "paste_delay", c.output.paste_delay);
    get("output", "ending_action", c.output.ending_action);
    get("output", "lowercase", c.output.lowercase);
    get("output", "also_copy_to_clipboard", c.output.also_copy_to_clipboard);

    get("formatting", "remove_fillers", c.formatting.remove_fillers);
    get("formatting", "spoken_commands", c.formatting.spoken_commands);
    get("formatting", "dictionary", c.formatting.dictionary);

    get("feedback", "enabled", c.feedback.enabled);
    get("feedback", "frequency_start", c.feedback.frequency_start);
    get("feedback", "frequency_stop", c.feedback.frequency_stop);
    get("feedback", "frequency_error", c.feedback.frequency_error);
    get("feedback", "duration", c.feedback.duration);
    get("feedback", "volume", c.feedback.volume);

    get("daemon", "log_level", c.daemon.log_level);
    if (j.contains("daemon") && j["daemon"].contains("log_file")) {
        auto s = j["daemon"]["log_file"].get<std::string>();
        c.daemon.log_file = (s.empty() || s == "default")
                              ? std::nullopt : std::optional<std::string>(s);
    }
    get("daemon", "pid_file", c.daemon.pid_file);
    get("daemon", "work_dir", c.daemon.work_dir);

    get("tray", "enabled", c.tray.enabled);

    get("avatar", "enabled", c.avatar.enabled);
    get("avatar", "character", c.avatar.character);
    get("avatar", "size", c.avatar.size);

    return c;
}

nlohmann::json get_config_json(const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return config_to_json(Config::default_config());
    }
    Config c = Config::load(path);
    return config_to_json(c);
}

nlohmann::json defaults_json() {
    return config_to_json(Config::default_config());
}

nlohmann::json models_json() {
    nlohmann::json models = nlohmann::json::array();
    for (int i = 0; i < MODEL_COUNT; ++i) {
        const ModelInfo& m = MODELS[i];
        models.push_back({
            {"name", m.name},
            {"description", m.description},
            {"size", m.size},
            {"speed", m.speed},
            {"english_only", m.english_only},
            // The one truth both the prompt and the ready badge must use.
            {"downloaded", is_model_downloaded(m.name)},
        });
    }
    return {
        {"cache_dir", get_cache_dir()},
        {"models", std::move(models)},
    };
}

nlohmann::json permissions_json() {
#ifdef __APPLE__
    return nlohmann::json::parse(aw_macos_permissions_status_json());
#else
    // No per-app TCC model on Linux/Windows; the UI hides the pane.
    return {
        {"applicable", false},
        {"permissions", nlohmann::json::array()},
    };
#endif
}

ValidationResult validate_json(const nlohmann::json& j) {
    ValidationResult r;
    Config c;
    try {
        c = json_to_config(j);
    } catch (const JsonFieldError& e) {
        ValidationIssue iss;
        iss.severity = ValidationSeverity::Error;
        iss.path = e.path();
        iss.code = "type_error";
        iss.message = e.what();
        r.errors.emplace_back(iss.message);
        r.issues.push_back(std::move(iss));
        return r;
    } catch (const std::exception& e) {
        ValidationIssue iss;
        iss.severity = ValidationSeverity::Error;
        iss.code = "type_error";
        iss.message = e.what();
        r.errors.emplace_back(iss.message);
        r.issues.push_back(std::move(iss));
        return r;
    }

    auto issues = c.validate_all();
    for (auto& iss : issues) {
        if (iss.severity == ValidationSeverity::Error) {
            r.errors.emplace_back(iss.message);
        }
        r.issues.push_back(std::move(iss));
    }
    return r;
}

nlohmann::json issue_to_json(const ValidationIssue& issue) {
    return nlohmann::json{
        {"severity", issue.severity == ValidationSeverity::Error ? "error" : "warning"},
        {"path", issue.path},
        {"code", issue.code},
        {"message", issue.message},
    };
}

void save_config_json(const std::string& path, const nlohmann::json& j) {
    Config c = json_to_config(j);
    c.save(path);
}

}  // namespace autowhisper::settings

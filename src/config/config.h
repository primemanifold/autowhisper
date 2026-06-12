#pragma once

#include <optional>
#include <string>
#include <vector>

namespace autowhisper {

struct ModelConfig {
    std::string size = "distil-large-v3";
    std::string device = "cuda";
    std::string compute_type = "float16";
    int beam_size = 1;
    std::string language = "en";
    int num_threads = 4;
};

struct AudioConfig {
    int sample_rate = 16000;
    int channels = 1;
    int buffer_size = 1024;
    std::optional<std::string> device;         // Input device
    std::optional<std::string> output_device;  // Output device (feedback)
    bool vad_enabled = true;
    float vad_threshold = 0.5f;
    float silence_duration = 0.3f;
    float max_duration = 600.0f;
    bool mute_other_apps = false;
};

struct HotkeyConfig {
    std::string mode = "push_to_talk";
    std::vector<std::string> trigger = {"shift+super"};
    std::vector<std::string> cancel = {"esc"};
    bool escape_to_cancel = true;
};

struct OutputConfig {
    std::string method = "inject";
    bool auto_paste = true;
    float paste_delay = 0.05f;
    std::string ending_action = "none";  // "none", "newline", "return_key"
    bool lowercase = false;
    bool also_copy_to_clipboard = true;
};

struct FormattingConfig {
    bool remove_fillers = true;
    bool spoken_commands = true;
    // Personal dictionary entries, "spoken => written".
    std::vector<std::string> dictionary;
};

struct FeedbackConfig {
    bool enabled = true;
    int frequency_start = 800;
    int frequency_stop = 400;
    int frequency_error = 600;
    float duration = 0.1f;
    float volume = 0.3f;
};

struct DaemonConfig {
    std::string log_level = "info";
    std::optional<std::string> log_file;
    std::string pid_file = "/tmp/autowhisper.pid";
    std::string work_dir = "/opt/autowhisper";
};

struct TrayConfig {
    bool enabled = true;
};

enum class ValidationSeverity {
    Error,
    Warning,
};

struct ValidationIssue {
    ValidationSeverity severity = ValidationSeverity::Error;
    std::string path;
    std::string code;
    std::string message;
};

struct ConfigLoadResult;
struct Config;

struct ConfigValidator {
    static std::vector<ValidationIssue> validate(const Config& config);
};

struct Config {
    ModelConfig model;
    AudioConfig audio;
    HotkeyConfig hotkeys;
    OutputConfig output;
    FormattingConfig formatting;
    FeedbackConfig feedback;
    DaemonConfig daemon;
    TrayConfig tray;

    static Config load(const std::string& path);
    static ConfigLoadResult load_with_diagnostics(const std::string& path);
    static Config default_config();
    std::vector<ValidationIssue> validate_all() const;
    void validate() const;
    void save(const std::string& path) const;
};

struct ConfigLoadResult {
    Config config;
    std::vector<ValidationIssue> issues;
};

// Find config file in standard locations
std::string find_config_file();

// Get user config path (~/.config/autowhisper/config.toml)
std::string get_user_config_path();

// Resolve the active config path: user config if present, else first
// match from find_config_file(), else the user-config path (for writes).
std::string resolve_config_path();

// Ensure a writable per-user config exists. If the user config is absent,
// copy the bundled/default config when available, otherwise write defaults.
std::string ensure_user_config_file();

// The shortest double whose decimal form round-trips the float exactly.
// Used wherever config floats are serialized (TOML, settings JSON) so
// 0.05f surfaces as 0.05, never 0.05000000074505806.
double shortest_double(float v);

} // namespace autowhisper

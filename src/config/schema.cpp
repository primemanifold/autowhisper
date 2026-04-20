#include "config/schema.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace autowhisper::schema {

namespace {

using V = std::vector<std::string_view>;

// The canonical key table. Order matches config.toml section order.
const std::vector<KeyDef>& table() {
    static const std::vector<KeyDef> t = {
        // [model]
        {"model", "size", Type::Enum,
            V{"tiny", "tiny.en", "base", "base.en", "small", "small.en",
              "medium", "medium.en", "large", "large-v1", "large-v2", "large-v3",
              "distil-large-v2", "distil-large-v3", "distil-medium.en", "distil-small.en"},
            std::nullopt, std::nullopt,
            "Whisper model variant. Smaller = faster, larger = more accurate."},
        {"model", "device", Type::Enum,
            V{"cuda", "cpu", "auto"},
            std::nullopt, std::nullopt,
            "Inference device."},
        {"model", "compute_type", Type::Enum,
            V{"float16", "float32", "int8", "int8_float16",
              "int8_float32", "int8_bfloat16", "bfloat16"},
            std::nullopt, std::nullopt,
            "Numeric precision for inference."},
        {"model", "beam_size", Type::Int, V{}, 1.0, std::nullopt,
            "Beam search width. 1 = greedy."},
        {"model", "language", Type::String, V{}, std::nullopt, std::nullopt,
            "ISO 639-1 language code (e.g. 'en')."},
        {"model", "num_threads", Type::Int, V{}, 1.0, std::nullopt,
            "CPU threads for inference."},

        // [audio]
        {"audio", "sample_rate", Type::Int, V{}, 1.0, std::nullopt,
            "Capture sample rate in Hz. Whisper expects 16000."},
        {"audio", "channels", Type::Int, V{}, 1.0, std::nullopt,
            "Capture channel count."},
        {"audio", "buffer_size", Type::Int, V{}, 1.0, std::nullopt,
            "Audio buffer size in frames."},
        {"audio", "device", Type::String, V{}, std::nullopt, std::nullopt,
            "Input device name. Empty = default."},
        {"audio", "output_device", Type::String, V{}, std::nullopt, std::nullopt,
            "Output device for feedback tones. Empty = default."},
        {"audio", "vad_enabled", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Enable voice activity detection."},
        {"audio", "vad_threshold", Type::Float, V{}, 0.0, 1.0,
            "VAD activation threshold, 0..1."},
        {"audio", "silence_duration", Type::Float, V{}, 0.0, std::nullopt,
            "Silence duration in seconds before stop (toggle mode)."},
        {"audio", "max_duration", Type::Float, V{}, 0.0, std::nullopt,
            "Maximum recording length in seconds."},
        {"audio", "mute_other_apps", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Mute other applications while recording."},

        // [hotkeys]
        {"hotkeys", "mode", Type::Enum,
            V{"push_to_talk", "toggle"},
            std::nullopt, std::nullopt,
            "Hotkey activation mode."},
        {"hotkeys", "trigger", Type::StringArray, V{}, std::nullopt, std::nullopt,
            "One or more trigger hotkeys, e.g. ['shift+super']."},
        {"hotkeys", "cancel", Type::StringArray, V{}, std::nullopt, std::nullopt,
            "Cancel hotkeys, e.g. ['esc']."},
        {"hotkeys", "escape_to_cancel", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Treat Escape as cancel."},

        // [output]
        {"output", "method", Type::Enum,
            V{"inject", "clipboard"},
            std::nullopt, std::nullopt,
            "How transcribed text reaches the cursor."},
        {"output", "auto_paste", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Automatically paste after clipboard write."},
        {"output", "paste_delay", Type::Float, V{}, 0.0, std::nullopt,
            "Delay before paste, in seconds."},
        {"output", "ending_action", Type::Enum,
            V{"none", "newline", "return_key"},
            std::nullopt, std::nullopt,
            "What to append after the transcribed text."},
        {"output", "lowercase", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Lowercase the output."},
        {"output", "also_copy_to_clipboard", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Copy to clipboard in addition to the primary method."},

        // [feedback]
        {"feedback", "enabled", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Play tones on record start/stop/error."},
        {"feedback", "frequency_start", Type::Int, V{}, 1.0, std::nullopt,
            "Start tone frequency in Hz."},
        {"feedback", "frequency_stop", Type::Int, V{}, 1.0, std::nullopt,
            "Stop tone frequency in Hz."},
        {"feedback", "frequency_error", Type::Int, V{}, 1.0, std::nullopt,
            "Error tone frequency in Hz."},
        {"feedback", "duration", Type::Float, V{}, 0.0, std::nullopt,
            "Tone duration in seconds."},
        {"feedback", "volume", Type::Float, V{}, 0.0, 1.0,
            "Tone volume, 0..1."},

        // [daemon]
        {"daemon", "log_level", Type::Enum,
            V{"trace", "debug", "info", "warn", "error", "critical", "off"},
            std::nullopt, std::nullopt,
            "spdlog level."},
        {"daemon", "log_file", Type::String, V{}, std::nullopt, std::nullopt,
            "Optional log file path. Empty or 'default' = none."},
        {"daemon", "pid_file", Type::String, V{}, std::nullopt, std::nullopt,
            "PID file path. Must be non-empty."},
        {"daemon", "work_dir", Type::String, V{}, std::nullopt, std::nullopt,
            "Working directory. Must be non-empty."},

        // [tray]
        {"tray", "enabled", Type::Bool, V{}, std::nullopt, std::nullopt,
            "Show the system tray icon."},
    };
    return t;
}

}  // namespace

const std::vector<KeyDef>& all() {
    return table();
}

const KeyDef* find(std::string_view section, std::string_view key) {
    const auto& t = table();
    auto it = std::find_if(t.begin(), t.end(), [&](const KeyDef& d) {
        return d.section == section && d.key == key;
    });
    return (it == t.end()) ? nullptr : &*it;
}

nlohmann::json to_json() {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& d : table()) {
        nlohmann::json entry;
        entry["key"] = std::string(d.key);
        switch (d.type) {
            case Type::String:      entry["type"] = "string"; break;
            case Type::Int:         entry["type"] = "int"; break;
            case Type::Float:       entry["type"] = "float"; break;
            case Type::Bool:        entry["type"] = "bool"; break;
            case Type::StringArray: entry["type"] = "string_array"; break;
            case Type::Enum:        entry["type"] = "enum"; break;
        }
        if (d.type == Type::Enum) {
            auto arr = nlohmann::json::array();
            for (auto v : d.enum_values) arr.push_back(std::string(v));
            entry["enum_values"] = std::move(arr);
        }
        entry["min_numeric"] = d.min_numeric ? nlohmann::json(*d.min_numeric) : nlohmann::json(nullptr);
        entry["max_numeric"] = d.max_numeric ? nlohmann::json(*d.max_numeric) : nlohmann::json(nullptr);
        entry["description"] = std::string(d.description);

        const std::string section_s(d.section);
        if (!out.contains(section_s)) out[section_s] = nlohmann::json::array();
        out[section_s].push_back(std::move(entry));
    }
    return out;
}

}  // namespace autowhisper::schema

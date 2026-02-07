#pragma once

#include "config/config.h"

#include <string>
#include <vector>

struct whisper_context;

namespace autowhisper {

class WhisperInference {
public:
    explicit WhisperInference(const ModelConfig& config);
    ~WhisperInference();

    WhisperInference(const WhisperInference&) = delete;
    WhisperInference& operator=(const WhisperInference&) = delete;

    void load();
    std::string transcribe(const std::vector<float>& audio, const std::string& language = "");
    bool is_loaded() const;
    float load_time() const { return load_time_; }

private:
    ModelConfig config_;
    whisper_context* ctx_ = nullptr;
    float load_time_ = 0.0f;

    std::string resolve_model_path() const;
};

} // namespace autowhisper

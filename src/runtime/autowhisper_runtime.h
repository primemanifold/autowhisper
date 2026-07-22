#pragma once

#include "config/config.h"
#include "inference/inference.h"
#include "runtime/transcription.h"

#include <memory>
#include <mutex>

namespace autowhisper {

class AutoWhisperRuntime final : public Transcriber {
public:
    explicit AutoWhisperRuntime(ModelConfig config);

    void load();
    bool is_loaded() const;
    const ModelConfig& model_config() const { return config_; }

    TranscriptionResult transcribe_file(
        const std::string& path,
        const TranscriptionOptions& options
    ) override;

private:
    ModelConfig config_;
    std::unique_ptr<WhisperInference> inference_;
    mutable std::mutex mutex_;
};

} // namespace autowhisper

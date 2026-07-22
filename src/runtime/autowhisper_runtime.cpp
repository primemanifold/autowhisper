#include "runtime/autowhisper_runtime.h"

#include "runtime/audio_file.h"

#include <chrono>
#include <exception>
#include <utility>

namespace autowhisper {

AutoWhisperRuntime::AutoWhisperRuntime(ModelConfig config)
    : config_(std::move(config)) {}

void AutoWhisperRuntime::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (inference_ && inference_->is_loaded()) return;
    auto inference = std::make_unique<WhisperInference>(config_);
    inference->load();
    inference_ = std::move(inference);
}

bool AutoWhisperRuntime::is_loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inference_ && inference_->is_loaded();
}

TranscriptionResult AutoWhisperRuntime::transcribe_file(
    const std::string& path,
    const TranscriptionOptions& options
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!inference_ || !inference_->is_loaded()) {
        return failed_transcription(
            options.request_id,
            "runtime_not_loaded",
            "AutoWhisper runtime is not loaded",
            true
        );
    }
    if (!options.model.empty() && options.model != config_.size) {
        return failed_transcription(
            options.request_id,
            "model_mismatch",
            "Requested model '" + options.model + "' is not loaded; restart the service with that model",
            true
        );
    }

    try {
        DecodedAudio audio = decode_audio_file(path);
        const auto started = std::chrono::steady_clock::now();
        const std::string language = options.language.empty()
            ? config_.language
            : options.language;
        std::string text = inference_->transcribe(audio.samples, language);
        const auto processing_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started
        ).count();

        TranscriptionResult result;
        result.request_id = options.request_id;
        result.status = text.empty()
            ? TranscriptionStatus::no_speech
            : TranscriptionStatus::completed;
        result.text = std::move(text);
        result.language = language == "auto" ? "" : language;
        result.duration_ms = audio.duration_ms();
        result.processing_ms = processing_ms;
        result.model = config_.size;
        if (!result.text.empty()) {
            result.segments.push_back({0, result.duration_ms, result.text});
        }
        return result;
    } catch (const std::invalid_argument& error) {
        return failed_transcription(
            options.request_id,
            "invalid_audio",
            error.what(),
            false
        );
    } catch (const std::exception& error) {
        return failed_transcription(
            options.request_id,
            "transcription_failed",
            error.what(),
            false
        );
    }
}

} // namespace autowhisper

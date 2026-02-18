#include "inference/inference.h"

#include <spdlog/spdlog.h>
#include <whisper.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace autowhisper {

WhisperInference::WhisperInference(const ModelConfig& config)
    : config_(config) {}

WhisperInference::~WhisperInference() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

std::string WhisperInference::resolve_model_path() const {
    // Map model name to GGML filename
    std::string model_name = config_.size;

    // Check common cache locations
    const char* home = std::getenv("HOME");
    std::string home_dir = home ? home : "/tmp";

    std::vector<std::string> search_paths;

    // Exact filename match first
    std::string ggml_name = "ggml-" + model_name + ".bin";
    search_paths.push_back(home_dir + "/.cache/whisper/" + ggml_name);
    search_paths.push_back(home_dir + "/.cache/whisper.cpp/" + ggml_name);
    search_paths.push_back("/usr/share/whisper/models/" + ggml_name);
    search_paths.push_back("/usr/local/share/whisper/models/" + ggml_name);

    // Also check for quantized variants
    // Map compute_type to GGML quantization
    std::string quant_suffix;
    if (config_.compute_type == "int8" || config_.compute_type == "int8_float16") {
        quant_suffix = "-q8_0";
    } else if (config_.compute_type == "float16" || config_.compute_type == "bfloat16") {
        quant_suffix = "-f16";
    }

    if (!quant_suffix.empty()) {
        std::string quant_name = "ggml-" + model_name + quant_suffix + ".bin";
        search_paths.insert(search_paths.begin() + 1,
                            home_dir + "/.cache/whisper/" + quant_name);
    }

    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            spdlog::info("Found model at: {}", path);
            return path;
        }
    }

    throw std::runtime_error(
        "Model '" + model_name + "' not found. Download with: autowhisper model download " + model_name);
}

void WhisperInference::load() {
    spdlog::info("Loading model '{}' with compute_type={}", config_.size, config_.compute_type);

    auto start = std::chrono::high_resolution_clock::now();

    std::string model_path = resolve_model_path();

    struct whisper_context_params cparams = whisper_context_default_params();

    // Enable GPU if requested
    if (config_.device == "cuda" || config_.device == "auto") {
        cparams.use_gpu = true;
    } else {
        cparams.use_gpu = false;
    }

    ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!ctx_) {
        throw std::runtime_error("Failed to load whisper model from: " + model_path);
    }

    auto end = std::chrono::high_resolution_clock::now();
    load_time_ = std::chrono::duration<float>(end - start).count();
    spdlog::info("Model loaded in {:.2f}s", load_time_);
}

std::string WhisperInference::transcribe(const std::vector<float>& audio,
                                          const std::string& language) {
    if (!ctx_) {
        throw std::runtime_error("Model not loaded. Call load() first.");
    }

    if (audio.empty()) {
        spdlog::warn("Empty audio buffer, skipping transcription");
        return "";
    }

    // Normalize if needed
    std::vector<float> normalized = audio;
    float max_val = 0.0f;
    for (float s : normalized) {
        max_val = std::max(max_val, std::abs(s));
    }
    if (max_val > 1.0f) {
        for (float& s : normalized) {
            s /= max_val;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Set up whisper parameters
    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.n_threads = config_.num_threads;
    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.no_timestamps = true;
    wparams.single_segment = false;

    // Language
    std::string lang = language.empty() ? config_.language : language;
    if (lang == "auto") {
        wparams.language = nullptr;  // Auto-detect
    } else {
        wparams.language = lang.c_str();
    }

    // Beam search
    if (config_.beam_size > 1) {
        wparams.strategy = WHISPER_SAMPLING_BEAM_SEARCH;
        wparams.beam_search.beam_size = config_.beam_size;
    } else {
        wparams.strategy = WHISPER_SAMPLING_GREEDY;
        wparams.greedy.best_of = 1;
    }

    wparams.temperature = 0.0f;
    wparams.no_speech_thold = 0.6f;

    // Run inference
    if (whisper_full(ctx_, wparams, normalized.data(), normalized.size()) != 0) {
        spdlog::error("Whisper inference failed");
        return "";
    }

    // Collect segments
    std::string result;
    int n_segments = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        if (text) {
            result += text;
        }
    }

    // Trim whitespace
    while (!result.empty() && std::isspace(result.front())) result.erase(result.begin());
    while (!result.empty() && std::isspace(result.back())) result.pop_back();

    auto end = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float>(end - start).count();
    float audio_duration = static_cast<float>(audio.size()) / WHISPER_SAMPLE_RATE;
    float rtf = (audio_duration > 0) ? elapsed / audio_duration : 0.0f;

    spdlog::debug("Transcription: {:.3f}s for {:.2f}s audio (RTF: {:.2f}x realtime)",
                  elapsed, audio_duration, rtf);

    return result;
}

bool WhisperInference::is_loaded() const {
    return ctx_ != nullptr;
}

} // namespace autowhisper

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/audio.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace autowhisper {

struct AudioManager::Impl {
    ma_device device{};
    ma_device_config device_config{};
    bool device_initialized = false;
    AudioManager* manager = nullptr;
};

AudioManager::AudioManager(const AudioConfig& config)
    : config_(config),
      max_samples_(static_cast<int>(config.max_duration * config.sample_rate)),
      silence_threshold_samples_(static_cast<int>(config.silence_duration * config.sample_rate)),
      impl_(std::make_unique<Impl>()) {
    impl_->manager = this;
}

AudioManager::~AudioManager() {
    if (impl_ && impl_->device_initialized) {
        ma_device_uninit(&impl_->device);
    }
}

void AudioManager::audio_callback(void* device_ptr, void* output, const void* input,
                                    unsigned int frame_count) {
    (void)output;
    auto* ma_dev = static_cast<ma_device*>(device_ptr);
    auto* mgr = static_cast<AudioManager*>(ma_dev->pUserData);

    if (!mgr || !mgr->recording_.load(std::memory_order_relaxed)) return;

    const auto* samples = static_cast<const float*>(input);
    if (!samples) return;

    // Copy audio data
    std::vector<float> chunk(samples, samples + frame_count);

    // Check total duration
    size_t total_samples = 0;
    {
        std::lock_guard<std::mutex> lock(mgr->lock_);
        for (const auto& buf : mgr->buffer_) {
            total_samples += buf.size();
        }
    }

    if (static_cast<int>(total_samples + chunk.size()) >= mgr->max_samples_) {
        spdlog::warn("Max recording duration reached");
        return;
    }

    std::lock_guard<std::mutex> lock(mgr->lock_);
    mgr->buffer_.push_back(std::move(chunk));
}

void AudioManager::initialize() {
    spdlog::info("Initializing audio: {}Hz, buffer_size={}", config_.sample_rate, config_.buffer_size);

    // Enumerate devices to get names
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) == MA_SUCCESS) {
        ma_device_info* playback_infos;
        ma_uint32 playback_count;
        ma_device_info* capture_infos;
        ma_uint32 capture_count;

        if (ma_context_get_devices(&context, &playback_infos, &playback_count,
                                    &capture_infos, &capture_count) == MA_SUCCESS) {
            // Find default input device name
            for (ma_uint32 i = 0; i < capture_count; i++) {
                if (capture_infos[i].isDefault) {
                    input_device_name_ = capture_infos[i].name;
                    spdlog::info("Default input device: {}", input_device_name_);
                    break;
                }
            }

            // Find default output device name
            for (ma_uint32 i = 0; i < playback_count; i++) {
                if (playback_infos[i].isDefault) {
                    output_device_name_ = playback_infos[i].name;
                    spdlog::info("Default output device: {}", output_device_name_);
                    break;
                }
            }
        }

        ma_context_uninit(&context);
    }
}

void AudioManager::start_recording() {
    std::lock_guard<std::mutex> lock(lock_);

    if (recording_.load()) {
        spdlog::warn("Already recording");
        return;
    }

    buffer_.clear();

    // Configure capture device
    impl_->device_config = ma_device_config_init(ma_device_type_capture);
    impl_->device_config.capture.format = ma_format_f32;
    impl_->device_config.capture.channels = config_.channels;
    impl_->device_config.sampleRate = config_.sample_rate;
    impl_->device_config.periodSizeInFrames = config_.buffer_size;
    impl_->device_config.dataCallback = [](ma_device* dev, void* out, const void* in, ma_uint32 frames) {
        audio_callback(dev, out, in, frames);
    };
    impl_->device_config.pUserData = this;

    if (ma_device_init(nullptr, &impl_->device_config, &impl_->device) != MA_SUCCESS) {
        spdlog::error("Failed to initialize capture device");
        return;
    }
    impl_->device_initialized = true;

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        spdlog::error("Failed to start capture device");
        ma_device_uninit(&impl_->device);
        impl_->device_initialized = false;
        return;
    }

    recording_.store(true);
    spdlog::debug("Recording started");
}

std::vector<float> AudioManager::stop_recording() {
    std::lock_guard<std::mutex> lock(lock_);

    if (!recording_.load()) {
        spdlog::warn("Not recording");
        return {};
    }

    recording_.store(false);

    if (impl_->device_initialized) {
        ma_device_stop(&impl_->device);
        ma_device_uninit(&impl_->device);
        impl_->device_initialized = false;
    }

    if (buffer_.empty()) return {};

    // Concatenate all chunks
    size_t total = 0;
    for (const auto& chunk : buffer_) total += chunk.size();

    std::vector<float> audio;
    audio.reserve(total);
    for (const auto& chunk : buffer_) {
        audio.insert(audio.end(), chunk.begin(), chunk.end());
    }
    buffer_.clear();

    spdlog::debug("Recording stopped: {} samples ({:.2f}s)", audio.size(),
                  static_cast<float>(audio.size()) / config_.sample_rate);

    return audio;
}

bool AudioManager::is_recording() const {
    return recording_.load();
}

float AudioManager::get_duration() const {
    std::lock_guard<std::mutex> lock(lock_);
    size_t total = 0;
    for (const auto& chunk : buffer_) total += chunk.size();
    return static_cast<float>(total) / config_.sample_rate;
}

std::vector<float> AudioManager::trim_silence(const std::vector<float>& audio, float threshold) const {
    if (audio.empty()) return audio;

    // Find first non-silent sample
    size_t start = 0;
    for (size_t i = 0; i < audio.size(); i++) {
        if (std::abs(audio[i]) > threshold) {
            start = i;
            break;
        }
    }

    // Find last non-silent sample
    size_t end = audio.size();
    for (size_t i = audio.size(); i > 0; i--) {
        if (std::abs(audio[i - 1]) > threshold) {
            end = i;
            break;
        }
    }

    if (start >= end) return {};

    // Add padding (50ms)
    int pad = static_cast<int>(0.05f * config_.sample_rate);
    start = (start > static_cast<size_t>(pad)) ? start - pad : 0;
    end = std::min(end + pad, audio.size());

    return std::vector<float>(audio.begin() + start, audio.begin() + end);
}

AudioDeviceList AudioManager::list_devices() {
    AudioDeviceList result;

    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
        spdlog::warn("Failed to initialize audio context for device enumeration");
        return result;
    }

    ma_device_info* playback_infos;
    ma_uint32 playback_count;
    ma_device_info* capture_infos;
    ma_uint32 capture_count;

    if (ma_context_get_devices(&context, &playback_infos, &playback_count,
                                &capture_infos, &capture_count) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < capture_count; i++) {
            result.inputs.push_back({static_cast<int>(i), capture_infos[i].name});
        }
        for (ma_uint32 i = 0; i < playback_count; i++) {
            result.outputs.push_back({static_cast<int>(i), playback_infos[i].name});
        }
    }

    ma_context_uninit(&context);
    return result;
}

} // namespace autowhisper

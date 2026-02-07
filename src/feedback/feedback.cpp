#include "feedback/feedback.h"

#include <spdlog/spdlog.h>
#include <miniaudio.h>

#include <cmath>
#include <thread>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace autowhisper {

FeedbackManager::FeedbackManager(const FeedbackConfig& config)
    : config_(config) {}

FeedbackManager::~FeedbackManager() = default;

void FeedbackManager::play_tone(int frequency, float duration, float volume) {
    if (!config_.enabled) return;

    // Generate sine wave with fade in/out
    int num_samples = static_cast<int>(sample_rate_ * duration);
    if (num_samples <= 0) return;

    std::vector<float> wave(num_samples);
    int fade_samples = static_cast<int>(0.01f * sample_rate_);  // 10ms fade

    for (int i = 0; i < num_samples; i++) {
        float t = static_cast<float>(i) / sample_rate_;
        float sample = std::sin(2.0f * static_cast<float>(M_PI) * frequency * t);

        // Apply fade envelope
        if (i < fade_samples) {
            sample *= static_cast<float>(i) / fade_samples;
        } else if (i > num_samples - fade_samples) {
            sample *= static_cast<float>(num_samples - i) / fade_samples;
        }

        wave[i] = sample * volume;
    }

    // Play in a detached thread
    std::thread([this, wave = std::move(wave)]() {
        std::lock_guard<std::mutex> lock(lock_);

        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 1;
        config.sampleRate = sample_rate_;

        // Use a simple blocking approach
        struct PlaybackData {
            const float* samples;
            size_t total_frames;
            size_t cursor;
        };

        PlaybackData data{wave.data(), wave.size(), 0};

        config.pUserData = &data;
        config.dataCallback = [](ma_device* dev, void* output, const void* /*input*/, ma_uint32 frames) {
            auto* pd = static_cast<PlaybackData*>(dev->pUserData);
            auto* out = static_cast<float*>(output);

            size_t frames_to_copy = std::min(static_cast<size_t>(frames), pd->total_frames - pd->cursor);
            if (frames_to_copy > 0) {
                std::memcpy(out, pd->samples + pd->cursor, frames_to_copy * sizeof(float));
                pd->cursor += frames_to_copy;
            }

            // Zero remaining
            if (frames_to_copy < frames) {
                std::memset(out + frames_to_copy, 0, (frames - frames_to_copy) * sizeof(float));
            }
        };

        ma_device device;
        if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
            spdlog::warn("Failed to init playback device for feedback");
            return;
        }

        if (ma_device_start(&device) != MA_SUCCESS) {
            ma_device_uninit(&device);
            spdlog::warn("Failed to start playback for feedback");
            return;
        }

        // Wait for playback to finish
        while (data.cursor < data.total_frames) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        // Small delay to let final buffer drain
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        ma_device_stop(&device);
        ma_device_uninit(&device);
    }).detach();
}

void FeedbackManager::play_start() {
    if (!config_.enabled) return;
    spdlog::debug("Playing start beep ({}Hz)", config_.frequency_start);
    play_tone(config_.frequency_start, config_.duration, config_.volume);
}

void FeedbackManager::play_stop() {
    if (!config_.enabled) return;
    spdlog::debug("Playing stop beep ({}Hz)", config_.frequency_stop);
    play_tone(config_.frequency_stop, config_.duration, config_.volume);
}

void FeedbackManager::play_error() {
    if (!config_.enabled) return;
    spdlog::debug("Playing error beep ({}Hz)", config_.frequency_error);
    play_tone(config_.frequency_error, config_.duration * 2.0f, config_.volume);
}

void FeedbackManager::play_short_beep() {
    if (!config_.enabled) return;
    spdlog::debug("Playing short beep (recording too short)");
    play_tone(300, config_.duration / 2.0f, config_.volume * 0.5f);
}

} // namespace autowhisper

#pragma once

#include "config/config.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace autowhisper {

struct AudioDevice {
    int index;
    std::string name;
};

struct AudioDeviceList {
    std::vector<AudioDevice> inputs;
    std::vector<AudioDevice> outputs;
};

class AudioManager {
public:
    explicit AudioManager(const AudioConfig& config);
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    void initialize();
    void start_recording();
    std::vector<float> stop_recording();
    bool is_recording() const;
    float get_duration() const;
    std::vector<float> trim_silence(const std::vector<float>& audio, float threshold = 0.01f) const;

    const std::string& input_device_name() const { return input_device_name_; }
    const std::string& output_device_name() const { return output_device_name_; }

    static AudioDeviceList list_devices();

private:
    AudioConfig config_;
    std::vector<std::vector<float>> buffer_;
    std::atomic<bool> recording_{false};
    mutable std::mutex lock_;
    std::string input_device_name_ = "Unknown";
    std::string output_device_name_ = "Unknown";
    int max_samples_;
    int silence_threshold_samples_;

    // miniaudio state (opaque, defined in .cpp)
    struct Impl;
    std::unique_ptr<Impl> impl_;

    static void audio_callback(void* device, void* output, const void* input,
                                unsigned int frame_count);
};

} // namespace autowhisper

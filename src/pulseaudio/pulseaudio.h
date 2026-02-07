#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace autowhisper {

class PulseAudioManager {
public:
    PulseAudioManager(bool enabled, float beep_duration);
    ~PulseAudioManager();

    PulseAudioManager(const PulseAudioManager&) = delete;
    PulseAudioManager& operator=(const PulseAudioManager&) = delete;

    bool initialize();
    void mute_other_apps(bool delay = true);
    void unmute_other_apps();
    void cleanup();

private:
    bool enabled_;
    float beep_duration_;
    bool initialized_ = false;
    std::mutex lock_;

    struct Impl;
    std::unique_ptr<Impl> impl_;

    void do_mute();
};

} // namespace autowhisper

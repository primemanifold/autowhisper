#pragma once

#include "config/config.h"

#include <mutex>

namespace autowhisper {

class FeedbackManager {
public:
    explicit FeedbackManager(const FeedbackConfig& config);
    ~FeedbackManager();

    FeedbackManager(const FeedbackManager&) = delete;
    FeedbackManager& operator=(const FeedbackManager&) = delete;

    void play_start();
    void play_stop();
    void play_error();
    void play_short_beep();

private:
    FeedbackConfig config_;
    int sample_rate_ = 44100;
    std::mutex lock_;

    void play_tone(int frequency, float duration, float volume);
};

} // namespace autowhisper

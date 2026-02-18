#pragma once

#include "config/config.h"
#include "audio/audio.h"
#include "feedback/feedback.h"
#include "hotkey/hotkey.h"
#include "inference/inference.h"
#include "output/output.h"
#include "pulseaudio/pulseaudio.h"
#include "tray/tray.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace autowhisper {

enum class DaemonState {
    IDLE,
    RECORDING,
    PROCESSING,
    SHUTDOWN,
};

class AutoWhisperDaemon {
public:
    AutoWhisperDaemon(Config config, const std::string& config_path = "");
    ~AutoWhisperDaemon();

    AutoWhisperDaemon(const AutoWhisperDaemon&) = delete;
    AutoWhisperDaemon& operator=(const AutoWhisperDaemon&) = delete;

    void initialize();
    void run();
    DaemonState state() const { return state_.load(); }

private:
    Config config_;
    std::string config_path_;
    std::atomic<DaemonState> state_{DaemonState::IDLE};

    // Event queue
    std::queue<HotkeyEvent> event_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> hotkey_reconfigure_requested_{false};

    // Components
    std::unique_ptr<AudioManager> audio_;
    std::unique_ptr<WhisperInference> whisper_;
    std::unique_ptr<OutputManager> output_;
    std::unique_ptr<FeedbackManager> feedback_;
    std::unique_ptr<PulseAudioManager> pulseaudio_;
    std::unique_ptr<HotkeyManager> hotkey_;
    std::unique_ptr<TrayManager> tray_;

    static constexpr float MIN_DURATION = 0.5f;

    void process_events();
    void handle_start();
    void handle_stop();
    void handle_cancel();
    void on_hotkey_event(HotkeyEvent event);
    void request_shutdown();
    void pause_hotkey();
    void resume_hotkey();
    void reconfigure_hotkey();
    void write_pid_file();
    void remove_pid_file();
    void cleanup();
    void setup_signals();
};

} // namespace autowhisper

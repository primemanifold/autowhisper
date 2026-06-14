#pragma once

#include "config/config.h"
#include "audio/audio.h"
#include "avatar/avatar.h"
#include "feedback/feedback.h"
#include "hotkey/hotkey.h"
#include "inference/inference.h"
#include "output/output.h"
#include "pipeline/transcript_pipeline.h"
#include "pulseaudio/pulseaudio.h"
#include "tray/tray.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
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

    // Live config reload: the settings UI writes config.toml from a separate
    // process, so the daemon polls the file's mtime and re-applies hotkey
    // changes to the running listener (a saved shortcut must work without a
    // restart). std::nullopt until the first poll captures the baseline.
    std::optional<std::filesystem::file_time_type> config_mtime_;
    void maybe_reload_config();

    // Event queue
    std::queue<HotkeyEvent> event_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_requested_{false};

    // Components
    std::unique_ptr<AudioManager> audio_;
    std::unique_ptr<WhisperInference> whisper_;
    std::unique_ptr<TranscriptPipeline> pipeline_;
    std::unique_ptr<OutputManager> output_;
    std::unique_ptr<FeedbackManager> feedback_;
    std::unique_ptr<PulseAudioManager> pulseaudio_;
    std::unique_ptr<HotkeyManager> hotkey_;
    std::unique_ptr<TrayManager> tray_;
    std::unique_ptr<AvatarManager> avatar_;

    static constexpr float MIN_DURATION = 0.5f;

    void process_events();
    void handle_start();
    void handle_stop();
    void handle_cancel();
    void on_hotkey_event(HotkeyEvent event);
    void request_shutdown();
    void write_pid_file();
    void remove_pid_file();
    void cleanup();
    void setup_signals();
    static bool other_audio_playing();
};

} // namespace autowhisper

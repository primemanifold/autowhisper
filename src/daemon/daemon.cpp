#include "daemon/daemon.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>

#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace autowhisper {

// Global pointer for signal handler
static AutoWhisperDaemon* g_daemon = nullptr;

AutoWhisperDaemon::AutoWhisperDaemon(Config config, const std::string& config_path)
    : config_(std::move(config)), config_path_(config_path) {}

AutoWhisperDaemon::~AutoWhisperDaemon() {
    if (g_daemon == this) g_daemon = nullptr;
}

void AutoWhisperDaemon::setup_signals() {
#ifndef _WIN32
    g_daemon = this;

    struct sigaction sa{};
    // Only use async-signal-safe operations in the handler.
    // spdlog, mutexes, and condition_variable::notify are NOT safe here.
    sa.sa_handler = [](int) {
        if (g_daemon) {
            g_daemon->shutdown_requested_.store(true, std::memory_order_release);
        }
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif
}

void AutoWhisperDaemon::initialize() {
    spdlog::info("Initializing AutoWhisper daemon");

    setup_signals();
    write_pid_file();

    // Initialize components
    spdlog::info("Initializing audio subsystem");
    audio_ = std::make_unique<AudioManager>(config_.audio);
    audio_->initialize();

    spdlog::info("Loading Whisper model");
    whisper_ = std::make_unique<WhisperInference>(config_.model);
    whisper_->load();

    spdlog::info("Initializing output subsystem");
    output_ = std::make_unique<OutputManager>(config_.output);
    output_->initialize();

    spdlog::info("Initializing feedback subsystem");
    feedback_ = std::make_unique<FeedbackManager>(config_.feedback);

    spdlog::info("Initializing PulseAudio manager");
    pulseaudio_ = std::make_unique<PulseAudioManager>(
        config_.audio.mute_other_apps, config_.feedback.duration);
    pulseaudio_->initialize();

    spdlog::info("Initializing hotkey manager");
    hotkey_ = std::make_unique<HotkeyManager>(config_.hotkeys,
        [this](HotkeyEvent e) { on_hotkey_event(e); });

    spdlog::info("Initializing tray icon");
    tray_ = std::make_unique<TrayManager>(
        config_.tray.enabled,
        [this]() { request_shutdown(); },   // on_quit
        [this]() { pause_hotkey(); },       // on_settings_open
        [this]() { resume_hotkey(); },      // on_settings_close
        config_path_);
    tray_->set_input_device(audio_->input_device_name());
    tray_->set_output_device(audio_->output_device_name());
    tray_->set_hotkey(config_.hotkeys.trigger);
    tray_->set_cancel_hotkey(config_.hotkeys.cancel);
    tray_->set_config(config_);
    tray_->start();

    spdlog::info("Initialization complete");
}

void AutoWhisperDaemon::run() {
    spdlog::info("Starting AutoWhisper daemon");

    hotkey_->start();

    while (!shutdown_requested_.load()) {
        process_events();
    }

    cleanup();
}

void AutoWhisperDaemon::process_events() {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // Wait with timeout for shutdown check
    queue_cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return !event_queue_.empty() ||
               shutdown_requested_.load() ||
               hotkey_reconfigure_requested_.load(std::memory_order_acquire);
    });

    if (hotkey_reconfigure_requested_.exchange(false, std::memory_order_acq_rel)) {
        lock.unlock();
        reconfigure_hotkey();
        return;
    }

    if (event_queue_.empty()) return;

    HotkeyEvent event = event_queue_.front();
    event_queue_.pop();
    lock.unlock();

    spdlog::debug("Processing event: {} in state {}",
                  event == HotkeyEvent::START ? "START" :
                  event == HotkeyEvent::STOP ? "STOP" : "CANCEL",
                  static_cast<int>(state_.load()));

    switch (event) {
        case HotkeyEvent::START: handle_start(); break;
        case HotkeyEvent::STOP: handle_stop(); break;
        case HotkeyEvent::CANCEL: handle_cancel(); break;
    }
}

void AutoWhisperDaemon::handle_start() {
    if (state_.load() != DaemonState::IDLE) {
        spdlog::debug("Ignoring START in state {}", static_cast<int>(state_.load()));
        return;
    }

    spdlog::info("Starting recording");
    state_.store(DaemonState::RECORDING);
    tray_->set_state(TrayState::RECORDING);
    feedback_->play_start();
    pulseaudio_->mute_other_apps(true);
    audio_->start_recording();
}

void AutoWhisperDaemon::handle_stop() {
    if (state_.load() != DaemonState::RECORDING) {
        spdlog::debug("Ignoring STOP in state {}", static_cast<int>(state_.load()));
        return;
    }

    spdlog::info("Stopping recording");
    state_.store(DaemonState::PROCESSING);
    tray_->set_state(TrayState::PROCESSING);
    feedback_->play_stop();
    pulseaudio_->unmute_other_apps();

    auto audio = audio_->stop_recording();
    float duration = static_cast<float>(audio.size()) / config_.audio.sample_rate;

    if (duration < MIN_DURATION) {
        spdlog::warn("Recording too short ({:.2f}s), skipping", duration);
        feedback_->play_short_beep();
        state_.store(DaemonState::IDLE);
        tray_->set_state(TrayState::IDLE);
        return;
    }

    // Trim silence
    audio = audio_->trim_silence(audio);
    if (audio.empty()) {
        spdlog::warn("No audio after silence trimming");
        state_.store(DaemonState::IDLE);
        tray_->set_state(TrayState::IDLE);
        return;
    }

    // Transcribe
    try {
        spdlog::info("Transcribing {:.2f}s of audio", duration);
        std::string text = whisper_->transcribe(audio);

        if (!text.empty()) {
            std::string preview = text.size() > 50 ? text.substr(0, 50) + "..." : text;
            spdlog::info("Transcription: {}", preview);
            bool success = output_->inject(text);
            if (!success) {
                spdlog::warn("Text injection failed");
                feedback_->play_error();
            }
        } else {
            spdlog::info("Empty transcription result");
        }
    } catch (const std::exception& e) {
        spdlog::error("Transcription error: {}", e.what());
        feedback_->play_error();
    }

    state_.store(DaemonState::IDLE);
    tray_->set_state(TrayState::IDLE);
}

void AutoWhisperDaemon::handle_cancel() {
    auto current = state_.load();

    if (current == DaemonState::RECORDING) {
        spdlog::info("Canceling recording");
        audio_->stop_recording();
        pulseaudio_->unmute_other_apps();
        feedback_->play_error();
    } else if (current == DaemonState::PROCESSING) {
        spdlog::info("Cannot cancel during processing");
        return;
    }

    state_.store(DaemonState::IDLE);
    tray_->set_state(TrayState::IDLE);
}

void AutoWhisperDaemon::on_hotkey_event(HotkeyEvent event) {
    if (hotkeys_paused_.load(std::memory_order_acquire)) {
        spdlog::debug("Dropping hotkey event while settings dialog is open");
        return;
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (event_queue_.size() < 100) {
        event_queue_.push(event);
        queue_cv_.notify_one();
    } else {
        spdlog::warn("Event queue full, dropping event");
    }
}

void AutoWhisperDaemon::request_shutdown() {
    state_.store(DaemonState::SHUTDOWN);
    shutdown_requested_.store(true);
    queue_cv_.notify_all();
}

void AutoWhisperDaemon::pause_hotkey() {
    spdlog::info("Pausing hotkey handling");
    hotkeys_paused_.store(true, std::memory_order_release);

    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!event_queue_.empty()) {
        event_queue_.pop();
    }
}

void AutoWhisperDaemon::resume_hotkey() {
    spdlog::info("Scheduling hotkey listener resume");
    hotkey_reconfigure_requested_.store(true, std::memory_order_release);
    queue_cv_.notify_all();
}

void AutoWhisperDaemon::reconfigure_hotkey() {
    spdlog::info("Resuming hotkey listener");
    if (!config_path_.empty()) {
        try {
            Config new_config = Config::load(config_path_);
            config_.hotkeys = new_config.hotkeys;
            config_.audio = new_config.audio;
        } catch (const std::exception& e) {
            spdlog::error("Failed to reload config: {}", e.what());
        }
    }

    if (hotkey_) {
        hotkey_->stop();
    }

    hotkey_ = std::make_unique<HotkeyManager>(config_.hotkeys,
        [this](HotkeyEvent e) { on_hotkey_event(e); });
    tray_->set_hotkey(config_.hotkeys.trigger);
    tray_->set_cancel_hotkey(config_.hotkeys.cancel);
    hotkey_->start();
    hotkeys_paused_.store(false, std::memory_order_release);
}

void AutoWhisperDaemon::write_pid_file() {
    try {
        fs::path pid_path(config_.daemon.pid_file);
        fs::create_directories(pid_path.parent_path());
        std::ofstream ofs(pid_path);
        if (ofs) {
#ifndef _WIN32
            ofs << getpid();
#endif
            spdlog::debug("Wrote PID file to {}", config_.daemon.pid_file);
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to write PID file: {}", e.what());
    }
}

void AutoWhisperDaemon::remove_pid_file() {
    try {
        if (fs::exists(config_.daemon.pid_file)) {
            fs::remove(config_.daemon.pid_file);
            spdlog::debug("Removed PID file {}", config_.daemon.pid_file);
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to remove PID file: {}", e.what());
    }
}

void AutoWhisperDaemon::cleanup() {
    spdlog::info("Cleaning up");

    hotkey_->stop();
    tray_->stop();

    if (audio_->is_recording()) {
        audio_->stop_recording();
    }

    pulseaudio_->cleanup();
    remove_pid_file();

    spdlog::info("Cleanup complete");
}

} // namespace autowhisper

#include "daemon/daemon.h"

#include "util/subprocess.h"

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

#if defined(__APPLE__)
namespace autowhisper {
// Defined in daemon_macos.mm. Kept outside this TU so we don't need to
// compile daemon.cpp as Objective-C++ on mac.
void aw_macos_setup_signals(std::atomic<bool>* shutdown_flag);
void aw_macos_teardown_signals();
void aw_macos_run_event_loop(std::function<void()> worker);
void aw_macos_stop_event_loop();
} // namespace autowhisper
#endif

namespace fs = std::filesystem;

namespace autowhisper {

// Global pointer for signal handler
static AutoWhisperDaemon* g_daemon = nullptr;

AutoWhisperDaemon::AutoWhisperDaemon(Config config, const std::string& config_path)
    : config_(std::move(config)), config_path_(config_path) {}

AutoWhisperDaemon::~AutoWhisperDaemon() {
#if defined(__APPLE__)
    aw_macos_teardown_signals();
#endif
    if (g_daemon == this) g_daemon = nullptr;
}

void AutoWhisperDaemon::setup_signals() {
    g_daemon = this;
#if defined(__APPLE__)
    // Use dispatch sources — the signal handler path cannot safely call
    // [NSApp stop:] which is what request_shutdown() does on mac.
    aw_macos_setup_signals(&shutdown_requested_);
#elif !defined(_WIN32)
    struct sigaction sa{};
    sa.sa_handler = [](int) {
        if (g_daemon) {
            g_daemon->shutdown_requested_.store(true, std::memory_order_release);
        }
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, nullptr);
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

    pipeline_ = std::make_unique<TranscriptPipeline>(config_.formatting,
                                                     config_.model.language);

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
    hotkey_ = std::make_unique<HotkeyManager>(effective_hotkey_config(),
        [this](HotkeyEvent e) { on_hotkey_event(e); });

    if (config_.avatar.enabled) {
        spdlog::info("Initializing companion avatar");
        avatar_ = std::make_unique<AvatarManager>(
            config_.avatar,
            [this]() { return audio_ ? audio_->peak() : 0.f; },
            []() { return other_audio_playing(); });
        // Clicking the floating companion toggles dictation, the same as the
        // hotkey: START when idle, STOP while recording. Queued like any
        // hotkey event so it runs on the daemon thread.
        avatar_->on_toggle([this]() {
            if (state_.load() != DaemonState::RECORDING) {
                on_hotkey_event(HotkeyEvent::START);
            } else {
                on_hotkey_event(active_mode_ == CaptureMode::ASK_FABRIC
                                    ? HotkeyEvent::ASK_STOP
                                    : HotkeyEvent::STOP);
            }
        });
        avatar_->start();
        avatar_->set_state(AvatarState::Idle);
    }

    spdlog::info("Initializing tray icon");
    tray_ = std::make_unique<TrayManager>(
        config_.tray.enabled,
        [this]() { request_shutdown(); },
        config_path_);
    // Start first so the status-item + menu exist before setters fire.
    tray_->start();
    tray_->set_input_device(audio_->input_device_name());
    tray_->set_output_device(audio_->output_device_name());
    tray_->set_hotkey(config_.hotkeys.trigger);
    tray_->set_ask_hotkey(config_.fabric.enabled
                              ? config_.hotkeys.ask_trigger
                              : std::vector<std::string>{});
    tray_->set_cancel_hotkey(config_.hotkeys.cancel);

    spdlog::info("Initialization complete");
}

void AutoWhisperDaemon::run() {
    spdlog::info("Starting AutoWhisper daemon");

    hotkey_->start();

#if defined(__APPLE__)
    // AppKit requires the main thread and a running NSApp (for the tray).
    // Move event-loop work to a worker and call NSApp.run() here.
    aw_macos_run_event_loop([this]() {
        while (!shutdown_requested_.load()) {
            process_events();
        }
    });
#else
    while (!shutdown_requested_.load()) {
        process_events();
    }
#endif

    cleanup();
}

void AutoWhisperDaemon::maybe_reload_config() {
    // Reconfiguring the matcher clears its active press. Defer reload until
    // idle so releasing a held push-to-talk key can always deliver STOP.
    if (state_.load() != DaemonState::IDLE) return;

    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(config_path_, ec);
    if (ec) return;  // file gone/unreadable this tick; try again next time
    if (!config_mtime_) { config_mtime_ = mtime; return; }  // baseline
    if (mtime == *config_mtime_) return;
    config_mtime_ = mtime;

    Config fresh;
    try {
        fresh = Config::load(config_path_);
    } catch (const std::exception& e) {
        spdlog::warn("Config changed but failed to reload: {}", e.what());
        return;
    }

    // Hotkeys and the opt-in Fabric bridge are applied live. Other settings
    // (model, audio, output) still take effect on the next run.
    const bool hotkeys_changed =
        fresh.hotkeys.trigger != config_.hotkeys.trigger ||
        fresh.hotkeys.ask_trigger != config_.hotkeys.ask_trigger ||
        fresh.hotkeys.cancel != config_.hotkeys.cancel ||
        fresh.hotkeys.mode != config_.hotkeys.mode ||
        fresh.hotkeys.escape_to_cancel != config_.hotkeys.escape_to_cancel;
    const bool fabric_changed =
        fresh.fabric.enabled != config_.fabric.enabled ||
        fresh.fabric.executable != config_.fabric.executable ||
        fresh.fabric.timeout_seconds != config_.fabric.timeout_seconds;
    if (hotkeys_changed || fabric_changed) {
        spdlog::info("Config changed: applying Dictate and Ask Fabric bindings live");
        config_.hotkeys = fresh.hotkeys;
        config_.fabric = fresh.fabric;
        if (hotkey_) hotkey_->set_config(effective_hotkey_config());
        if (tray_) {
            tray_->set_hotkey(config_.hotkeys.trigger);
            tray_->set_ask_hotkey(config_.fabric.enabled
                                      ? config_.hotkeys.ask_trigger
                                      : std::vector<std::string>{});
            tray_->set_cancel_hotkey(config_.hotkeys.cancel);
        }
    }
}

HotkeyConfig AutoWhisperDaemon::effective_hotkey_config() const {
    HotkeyConfig effective = config_.hotkeys;
    if (!config_.fabric.enabled) effective.ask_trigger.clear();
    return effective;
}

void AutoWhisperDaemon::process_events() {
    // Checked before taking the queue lock: maybe_reload_config -> hotkey
    // set_config locks the hotkey mutex, and the hotkey callback locks the
    // queue mutex, so this must never run while holding queue_mutex_.
    maybe_reload_config();

    std::unique_lock<std::mutex> lock(queue_mutex_);

    queue_cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return !event_queue_.empty() || shutdown_requested_.load();
    });

    if (event_queue_.empty()) return;

    HotkeyEvent event = event_queue_.front();
    event_queue_.pop();
    lock.unlock();

    spdlog::debug("Processing hotkey event {} in state {}",
                  static_cast<int>(event), static_cast<int>(state_.load()));

    switch (event) {
        case HotkeyEvent::START: handle_start(CaptureMode::DICTATE); break;
        case HotkeyEvent::STOP: handle_stop(CaptureMode::DICTATE); break;
        case HotkeyEvent::ASK_START: handle_start(CaptureMode::ASK_FABRIC); break;
        case HotkeyEvent::ASK_STOP: handle_stop(CaptureMode::ASK_FABRIC); break;
        case HotkeyEvent::CANCEL: handle_cancel(); break;
    }
}

void AutoWhisperDaemon::handle_start(CaptureMode mode) {
    if (state_.load() != DaemonState::IDLE) {
        spdlog::debug("Ignoring START in state {}", static_cast<int>(state_.load()));
        return;
    }
    if (mode == CaptureMode::ASK_FABRIC && !config_.fabric.enabled) {
        spdlog::warn("Ignoring Ask Fabric hotkey because the integration is disabled");
        feedback_->play_error();
        return;
    }

    active_mode_ = mode;
    if (mode == CaptureMode::ASK_FABRIC) active_fabric_config_ = config_.fabric;
    spdlog::info("Starting {} recording",
                 mode == CaptureMode::ASK_FABRIC ? "Ask Fabric" : "Dictate");
    state_.store(DaemonState::RECORDING);
    tray_->set_state(mode == CaptureMode::ASK_FABRIC
                         ? TrayState::ASK_RECORDING
                         : TrayState::RECORDING);
    if (avatar_) avatar_->set_state(AvatarState::Summoned);
    feedback_->play_start();
    pulseaudio_->mute_other_apps(true);
    audio_->start_recording();
}

void AutoWhisperDaemon::handle_stop(CaptureMode mode) {
    if (state_.load() != DaemonState::RECORDING) {
        spdlog::debug("Ignoring STOP in state {}", static_cast<int>(state_.load()));
        return;
    }
    if (mode != active_mode_) {
        spdlog::debug("Ignoring STOP for a mode that did not start this recording");
        return;
    }

    spdlog::info("Stopping {} recording",
                 mode == CaptureMode::ASK_FABRIC ? "Ask Fabric" : "Dictate");
    state_.store(DaemonState::PROCESSING);
    tray_->set_state(mode == CaptureMode::ASK_FABRIC
                         ? TrayState::ASK_PROCESSING
                         : TrayState::PROCESSING);
    if (avatar_) avatar_->set_state(AvatarState::Thinking);
    feedback_->play_stop();
    pulseaudio_->unmute_other_apps();

    auto audio = audio_->stop_recording();
    float duration = static_cast<float>(audio.size()) / config_.audio.sample_rate;

    if (duration < MIN_DURATION) {
        spdlog::warn("Recording too short ({:.2f}s), skipping", duration);
        feedback_->play_short_beep();
        state_.store(DaemonState::IDLE);
        tray_->set_state(TrayState::IDLE);
        if (avatar_) avatar_->set_state(AvatarState::Idle);
        return;
    }

    // Trim silence
    audio = audio_->trim_silence(audio);
    if (audio.empty()) {
        spdlog::warn("No audio after silence trimming");
        state_.store(DaemonState::IDLE);
        tray_->set_state(TrayState::IDLE);
        if (avatar_) avatar_->set_state(AvatarState::Idle);
        return;
    }

    // Transcribe
    try {
        spdlog::info("Transcribing {:.2f}s of audio", duration);
        std::string text = whisper_->transcribe(audio);
        text = pipeline_->process(text);

        if (!text.empty()) {
            spdlog::info("Transcription completed ({} bytes)", text.size());
            if (avatar_) avatar_->set_state(AvatarState::Writing);
            bool success = false;
            if (mode == CaptureMode::ASK_FABRIC) {
                FabricAskResult answer = FabricClient(active_fabric_config_).ask(text);
                if (answer.ok()) {
                    spdlog::info("Ask Fabric completed ({} bytes)", answer.response.size());
                    success = output_->inject(answer.response);
                } else {
                    spdlog::warn("Ask Fabric failed: {}", answer.error);
                }
            } else {
                success = output_->inject(text);
            }
            if (!success) {
                spdlog::warn("{} output was not inserted",
                             mode == CaptureMode::ASK_FABRIC ? "Ask Fabric" : "Dictate");
                feedback_->play_error();
                if (avatar_) avatar_->set_state(AvatarState::Error);
            }
        } else {
            spdlog::info("Empty transcription result");
        }
    } catch (const std::exception& e) {
        spdlog::error("Transcription error: {}", e.what());
        feedback_->play_error();
        if (avatar_) avatar_->set_state(AvatarState::Error);
    }

    state_.store(DaemonState::IDLE);
    tray_->set_state(TrayState::IDLE);
    if (avatar_) avatar_->set_state(AvatarState::Idle);
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
    if (avatar_) avatar_->set_state(AvatarState::Idle);
}

void AutoWhisperDaemon::on_hotkey_event(HotkeyEvent event) {
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
#if defined(__APPLE__)
    // Unwind the NSApp run loop so run() can fall through to cleanup().
    aw_macos_stop_event_loop();
#endif
}

bool AutoWhisperDaemon::other_audio_playing() {
#if defined(__linux__)
    // PulseAudio/PipeWire: any active sink-input means something is playing.
    auto res = run_command({"pactl", "list", "short", "sink-inputs"}, 2);
    return res.exit_code == 0 && !res.stdout_str.empty();
#else
    return false;
#endif
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
    if (avatar_) avatar_->stop();
    tray_->stop();

    if (audio_->is_recording()) {
        audio_->stop_recording();
    }

    pulseaudio_->cleanup();
    remove_pid_file();

    spdlog::info("Cleanup complete");
}

} // namespace autowhisper

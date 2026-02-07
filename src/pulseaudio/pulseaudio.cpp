#include "pulseaudio/pulseaudio.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
#include <thread>

#ifdef HAVE_PULSEAUDIO
#include <pulse/pulseaudio.h>
#endif

namespace autowhisper {

struct MutedSinkInput {
    uint32_t index;
    std::string name;
    bool was_muted;
};

struct PulseAudioManager::Impl {
#ifdef HAVE_PULSEAUDIO
    pa_mainloop* mainloop = nullptr;
    pa_context* context = nullptr;
#endif
    std::vector<MutedSinkInput> muted_inputs;
    std::thread mute_timer;
    bool mute_timer_active = false;

    static const std::set<std::string> EXCLUDED_APPS;
};

const std::set<std::string> PulseAudioManager::Impl::EXCLUDED_APPS = {
    "python", "python3", "autowhisper", "sounddevice"
};

PulseAudioManager::PulseAudioManager(bool enabled, float beep_duration)
    : enabled_(enabled), beep_duration_(beep_duration), impl_(std::make_unique<Impl>()) {}

PulseAudioManager::~PulseAudioManager() {
    cleanup();
}

#ifdef HAVE_PULSEAUDIO

namespace {

// Simple blocking iteration helper
void iterate(pa_mainloop* ml, pa_operation* op) {
    if (!op) return;
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_mainloop_iterate(ml, 1, nullptr);
    }
    pa_operation_unref(op);
}

struct SinkInputListData {
    pa_mainloop* ml;
    std::vector<MutedSinkInput>* inputs_to_mute;
    const std::set<std::string>* excluded;
};

void sink_input_list_cb(pa_context* /*ctx*/, const pa_sink_input_info* info, int eol, void* userdata) {
    if (eol > 0 || !info) return;

    auto* data = static_cast<SinkInputListData*>(userdata);

    // Get binary name
    const char* binary = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);
    std::string binary_name = binary ? binary : "";

    // Extract basename
    auto pos = binary_name.rfind('/');
    if (pos != std::string::npos) {
        binary_name = binary_name.substr(pos + 1);
    }

    // Check exclusion
    if (data->excluded->count(binary_name)) {
        spdlog::debug("Excluding '{}' from muting (binary: {})", info->name, binary_name);
        return;
    }

    MutedSinkInput muted;
    muted.index = info->index;
    muted.name = info->name ? info->name : "Unknown";
    muted.was_muted = info->mute != 0;
    data->inputs_to_mute->push_back(muted);
}

} // anonymous namespace

bool PulseAudioManager::initialize() {
    if (!enabled_) {
        spdlog::debug("PulseAudio muting disabled by configuration");
        return true;
    }

    impl_->mainloop = pa_mainloop_new();
    if (!impl_->mainloop) {
        spdlog::warn("Failed to create PulseAudio mainloop. Muting disabled.");
        enabled_ = false;
        return true;
    }

    pa_mainloop_api* api = pa_mainloop_get_api(impl_->mainloop);
    impl_->context = pa_context_new(api, "autowhisper");
    if (!impl_->context) {
        spdlog::warn("Failed to create PulseAudio context. Muting disabled.");
        pa_mainloop_free(impl_->mainloop);
        impl_->mainloop = nullptr;
        enabled_ = false;
        return true;
    }

    pa_context_connect(impl_->context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    // Wait for connection
    while (true) {
        pa_mainloop_iterate(impl_->mainloop, 1, nullptr);
        auto state = pa_context_get_state(impl_->context);
        if (state == PA_CONTEXT_READY) break;
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
            spdlog::warn("PulseAudio connection failed. Muting disabled.");
            pa_context_unref(impl_->context);
            pa_mainloop_free(impl_->mainloop);
            impl_->context = nullptr;
            impl_->mainloop = nullptr;
            enabled_ = false;
            return true;
        }
    }

    initialized_ = true;
    spdlog::info("PulseAudio muting enabled");
    return true;
}

void PulseAudioManager::do_mute() {
    if (!enabled_ || !initialized_ || !impl_->context) return;

    std::lock_guard<std::mutex> lock(lock_);

    // List sink inputs
    std::vector<MutedSinkInput> to_mute;
    SinkInputListData data{impl_->mainloop, &to_mute, &Impl::EXCLUDED_APPS};

    auto* op = pa_context_get_sink_input_info_list(impl_->context, sink_input_list_cb, &data);
    iterate(impl_->mainloop, op);

    int muted_count = 0;
    for (auto& si : to_mute) {
        impl_->muted_inputs.push_back(si);
        if (!si.was_muted) {
            auto* mop = pa_context_set_sink_input_mute(impl_->context, si.index, 1, nullptr, nullptr);
            iterate(impl_->mainloop, mop);
            muted_count++;
            spdlog::debug("Muted: {}", si.name);
        }
    }

    if (muted_count > 0) {
        spdlog::debug("Muted {} other app(s)", muted_count);
    }
}

void PulseAudioManager::mute_other_apps(bool delay) {
    if (!enabled_ || !initialized_) return;

    // Cancel pending timer
    if (impl_->mute_timer_active && impl_->mute_timer.joinable()) {
        impl_->mute_timer_active = false;
        impl_->mute_timer.join();
    }

    if (delay) {
        float delay_seconds = beep_duration_ + 0.05f;
        spdlog::debug("Scheduling app mute in {:.2f}s", delay_seconds);
        impl_->mute_timer_active = true;
        impl_->mute_timer = std::thread([this, delay_seconds]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(delay_seconds * 1000)));
            if (impl_->mute_timer_active) {
                do_mute();
            }
            impl_->mute_timer_active = false;
        });
    } else {
        do_mute();
    }
}

void PulseAudioManager::unmute_other_apps() {
    if (!enabled_ || !initialized_ || !impl_->context) return;

    // Cancel pending mute
    if (impl_->mute_timer_active) {
        impl_->mute_timer_active = false;
        if (impl_->mute_timer.joinable()) impl_->mute_timer.join();
    }

    std::lock_guard<std::mutex> lock(lock_);

    if (impl_->muted_inputs.empty()) return;

    int restored = 0;
    for (const auto& si : impl_->muted_inputs) {
        if (!si.was_muted) {
            auto* op = pa_context_set_sink_input_mute(impl_->context, si.index, 0, nullptr, nullptr);
            if (op) {
                iterate(impl_->mainloop, op);
                restored++;
                spdlog::debug("Unmuted: {}", si.name);
            }
        }
    }

    if (restored > 0) {
        spdlog::debug("Restored audio for {} app(s)", restored);
    }

    impl_->muted_inputs.clear();
}

void PulseAudioManager::cleanup() {
    if (impl_->mute_timer_active) {
        impl_->mute_timer_active = false;
        if (impl_->mute_timer.joinable()) impl_->mute_timer.join();
    }

    unmute_other_apps();

    if (impl_->context) {
        pa_context_disconnect(impl_->context);
        pa_context_unref(impl_->context);
        impl_->context = nullptr;
    }
    if (impl_->mainloop) {
        pa_mainloop_free(impl_->mainloop);
        impl_->mainloop = nullptr;
    }

    initialized_ = false;
    spdlog::debug("PulseAudio manager cleaned up");
}

#else // !HAVE_PULSEAUDIO

bool PulseAudioManager::initialize() {
    if (enabled_) {
        spdlog::warn("PulseAudio support not compiled in. Muting disabled.");
        enabled_ = false;
    }
    return true;
}

void PulseAudioManager::do_mute() {}
void PulseAudioManager::mute_other_apps(bool /*delay*/) {}
void PulseAudioManager::unmute_other_apps() {}
void PulseAudioManager::cleanup() { spdlog::debug("PulseAudio manager cleaned up (stub)"); }

#endif // HAVE_PULSEAUDIO

} // namespace autowhisper

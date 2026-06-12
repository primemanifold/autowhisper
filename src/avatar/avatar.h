#pragma once

#include "config/config.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace autowhisper {

// The floating companion ("Echo" by default — the nymph who can only repeat
// the words of others, which is precisely this product's job). All motion
// math and every pixel are produced by shared, unit-tested code below;
// platform layers only create a window, blit the BGRA buffer, and pump a
// ~30fps timer. That is what keeps the three desktops pixel-identical.

enum class AvatarState {
    Hidden,
    Idle,       // afloat, breathing
    Summoned,   // hotkey pressed: glow ramps in, auto-advances to Listening
    Listening,  // recording: halo follows the live microphone level
    Thinking,   // transcribing: a mote orbits the ring
    Writing,    // text is being inserted: strokes are written beneath the orb
    Ambient,    // something else is playing audio: quiet shimmer
    Error,
};

// A mythological form: accent hue and tempo. The shape is shared — the
// spirit changes character, not species.
struct AvatarCharacter {
    const char* id;
    const char* name;
    const char* epithet;
    // Accent color (straight alpha applied at raster time), 0..1.
    float accent_r, accent_g, accent_b;
    // Multiplier on every animation period (>1 = calmer).
    float tempo;
};

const AvatarCharacter* find_character(const std::string& id);
extern const AvatarCharacter AVATAR_CHARACTERS[];
extern const int AVATAR_CHARACTER_COUNT;

// Deterministic animation state. Owns the latch rules (Summoned advances
// after 0.45s, Writing holds at least 1.4s) so platforms cannot drift.
class AvatarStateMachine {
public:
    void request(AvatarState s, double now_seconds);
    AvatarState effective(double now_seconds) const;
    // Seconds since the effective state began.
    double phase(double now_seconds) const;

private:
    AvatarState requested_ = AvatarState::Idle;
    mutable AvatarState shown_ = AvatarState::Idle;
    mutable double shown_since_ = 0.0;
    double writing_until_ = -1.0;
    double summoned_until_ = -1.0;
};

// Rasterizes one frame into a premultiplied BGRA buffer of size*size pixels
// (the byte order every target wants: X11 ARGB32, Win32 layered windows,
// CoreGraphics with kCGBitmapByteOrder32Little|PremultipliedFirst).
// `level` is the live microphone peak 0..1; `t` is seconds.
void avatar_rasterize(uint32_t* buf, int size, const AvatarCharacter& ch,
                      AvatarState state, double t, double phase, float level);

// Short uppercase caption shown beneath the orb (the floating-button state
// label). Empty string for Idle/Hidden.
const char* avatar_state_label(AvatarState s);

// Thread-safe tick engine shared by every platform shell: owns the state
// machine, polls the providers, and answers "what do I draw right now".
// Shells call request() from the daemon thread and tick() from their UI
// thread; everything else stays in tested common code.
class AvatarTicker {
public:
    AvatarTicker(AvatarStateMachine machine,
                 std::function<float()> level,
                 std::function<bool()> ambient);

    void request(AvatarState s);

    struct Snapshot {
        AvatarState state;
        double t;
        double phase;
        float level;
    };
    Snapshot tick();

private:
    AvatarStateMachine machine_;
    std::function<float()> level_;
    std::function<bool()> ambient_;
    std::atomic<int> requested_{int(AvatarState::Idle)};
    std::atomic<bool> dirty_{false};
    double start_time_ = -1.0;
    int ambient_countdown_ = 0;
    bool ambient_active_ = false;
};

class AvatarManager {
public:
    using LevelProvider = std::function<float()>;
    using AmbientProvider = std::function<bool()>;
    // Fired when the user clicks the companion (the Wispr-Flow-style
    // floating button): the daemon toggles dictation. Runs on the UI thread.
    using ToggleHandler = std::function<void()>;

    AvatarManager(const AvatarConfig& config, LevelProvider level,
                  AmbientProvider ambient);
    ~AvatarManager();

    AvatarManager(const AvatarManager&) = delete;
    AvatarManager& operator=(const AvatarManager&) = delete;

    void start();
    void stop();
    void set_state(AvatarState s);
    // Set before start(): a click on the orb invokes this.
    void on_toggle(ToggleHandler handler) { toggle_ = std::move(handler); }

private:
    AvatarConfig config_;
    LevelProvider level_;
    AmbientProvider ambient_;
    ToggleHandler toggle_;
    std::unique_ptr<AvatarTicker> ticker_;
    std::atomic<bool> running_{false};

    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend struct Impl;
};

} // namespace autowhisper

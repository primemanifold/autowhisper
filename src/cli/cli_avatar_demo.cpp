#include "avatar/avatar.h"
#include "cli/cli.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace autowhisper {

// `autowhisper avatar demo` — summons the companion and walks every state
// with a synthetic voice level. This is the runtime smoke we run on each
// desktop (Xvfb on Linux CI, Wine for the Windows artifact, a real desktop
// elsewhere); it needs no model, no microphone, and no hotkeys.
int cmd_avatar_demo(const std::string& character, int seconds) {
    AvatarConfig cfg;
    cfg.enabled = true;
    cfg.character = character;
    cfg.size = 112;

    const auto t0 = std::chrono::steady_clock::now();
    auto elapsed = [t0]() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
            .count();
    };

    AvatarManager avatar(
        cfg,
        // A synthetic "voice": slow swells with a little tremble.
        [elapsed]() {
            double t = elapsed();
            return float(std::clamp(
                0.45 + 0.4 * std::sin(t * 2.2) + 0.12 * std::sin(t * 9.0), 0.0, 1.0));
        },
        []() { return false; });

    const AvatarCharacter* ch = find_character(character);
    std::cout << ch->name << ", " << ch->epithet << ", appears.\n";
    avatar.start();

    struct Step { AvatarState s; const char* label; double hold; };
    const Step script[] = {
        {AvatarState::Idle,      "idle — afloat",                    2.0},
        {AvatarState::Summoned,  "summoned — the hotkey glow",       1.2},
        {AvatarState::Listening, "listening — halo follows voice",   4.0},
        {AvatarState::Thinking,  "thinking — transcribing",          2.5},
        {AvatarState::Writing,   "writing — putting it down",        3.0},
        {AvatarState::Ambient,   "ambient — music is playing",       2.5},
        {AvatarState::Error,     "error — amber blink",              1.6},
        {AvatarState::Idle,      "idle again",                       1.2},
    };

    double total = 0;
    for (const auto& st : script) total += st.hold;
    const double scale = seconds > 0 ? seconds / total : 1.0;

    for (const auto& st : script) {
        std::cout << "  " << st.label << "\n";
        avatar.set_state(st.s);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(int(st.hold * scale * 1000)));
    }

    avatar.stop();
    std::cout << ch->name << " rests.\n";
    return 0;
}

} // namespace autowhisper

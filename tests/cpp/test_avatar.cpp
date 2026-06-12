#include <catch2/catch_test_macros.hpp>

#include "avatar/avatar.h"

#include <numeric>
#include <vector>

using namespace autowhisper;

namespace {

// Sum of alpha across the buffer — a cheap "how much was drawn" probe.
uint64_t painted(const std::vector<uint32_t>& buf) {
    uint64_t sum = 0;
    for (uint32_t px : buf) sum += px >> 24;
    return sum;
}

std::vector<uint32_t> render(AvatarState s, double t, double phase, float level,
                             const char* who = "echo", int size = 96) {
    std::vector<uint32_t> buf(size_t(size) * size, 0xDEADBEEF);
    avatar_rasterize(buf.data(), size, *find_character(who), s, t, phase, level);
    return buf;
}

}  // namespace

// ---- pantheon ----

TEST_CASE("character registry resolves ids and falls back to Echo", "[avatar]") {
    REQUIRE(AVATAR_CHARACTER_COUNT == 3);
    CHECK(std::string(find_character("echo")->name) == "Echo");
    CHECK(std::string(find_character("hermes")->name) == "Hermes");
    CHECK(std::string(find_character("mnemosyne")->name) == "Mnemosyne");
    CHECK(std::string(find_character("zeus")->name) == "Echo");
    CHECK(std::string(find_character("")->name) == "Echo");
}

TEST_CASE("every character carries a usable accent and tempo", "[avatar]") {
    for (int i = 0; i < AVATAR_CHARACTER_COUNT; i++) {
        const auto& c = AVATAR_CHARACTERS[i];
        CHECK(c.tempo > 0.5f);
        CHECK(c.tempo < 2.0f);
        float lum = c.accent_r + c.accent_g + c.accent_b;
        CHECK(lum > 0.3f);
    }
}

// ---- state machine ----

TEST_CASE("summoned auto-advances to listening", "[avatar]") {
    AvatarStateMachine m;
    m.request(AvatarState::Summoned, 10.0);
    CHECK(m.effective(10.1) == AvatarState::Summoned);
    CHECK(m.effective(10.6) == AvatarState::Listening);
}

TEST_CASE("writing latches long enough to be seen", "[avatar]") {
    AvatarStateMachine m;
    m.request(AvatarState::Writing, 5.0);
    // The daemon flips back to Idle milliseconds after injection...
    m.request(AvatarState::Idle, 5.01);
    // ...but the scribe finishes her line.
    CHECK(m.effective(5.5) == AvatarState::Writing);
    CHECK(m.effective(6.0) == AvatarState::Writing);
    CHECK(m.effective(6.5) == AvatarState::Idle);
}

TEST_CASE("error and listening are immediate and unlatched", "[avatar]") {
    AvatarStateMachine m;
    m.request(AvatarState::Listening, 1.0);
    CHECK(m.effective(1.0) == AvatarState::Listening);
    m.request(AvatarState::Error, 2.0);
    CHECK(m.effective(2.0) == AvatarState::Error);
    m.request(AvatarState::Idle, 3.0);
    CHECK(m.effective(3.0) == AvatarState::Idle);
}

TEST_CASE("phase restarts when the effective state changes", "[avatar]") {
    AvatarStateMachine m;
    m.request(AvatarState::Idle, 0.0);
    m.effective(0.0);
    CHECK(m.phase(2.5) > 2.0);
    m.request(AvatarState::Listening, 3.0);
    m.effective(3.0);
    CHECK(m.phase(3.2) < 0.5);
}

// ---- renderer ----

TEST_CASE("hidden renders nothing, visible states render something", "[avatar]") {
    CHECK(painted(render(AvatarState::Hidden, 1.0, 0.5, 0.f)) == 0);
    for (AvatarState s : {AvatarState::Idle, AvatarState::Summoned,
                          AvatarState::Listening, AvatarState::Thinking,
                          AvatarState::Writing, AvatarState::Ambient,
                          AvatarState::Error}) {
        CHECK(painted(render(s, 1.0, 0.5, 0.5f)) > 10000);
    }
}

TEST_CASE("listening glow grows with the voice level", "[avatar]") {
    auto quiet = painted(render(AvatarState::Listening, 2.0, 1.0, 0.05f));
    auto loud = painted(render(AvatarState::Listening, 2.0, 1.0, 0.95f));
    CHECK(loud > quiet);
}

TEST_CASE("renderer is deterministic for identical inputs", "[avatar]") {
    auto a = render(AvatarState::Writing, 3.21, 0.8, 0.4f);
    auto b = render(AvatarState::Writing, 3.21, 0.8, 0.4f);
    CHECK(a == b);
}

TEST_CASE("writing animates strokes over its cycle", "[avatar]") {
    auto early = painted(render(AvatarState::Writing, 4.0, 0.20, 0.f));
    auto late = painted(render(AvatarState::Writing, 4.0, 1.30, 0.f));
    CHECK(late > early);  // more of the line has been written
}

TEST_CASE("premultiplied output never exceeds alpha", "[avatar]") {
    auto buf = render(AvatarState::Listening, 1.7, 0.9, 1.0f);
    for (uint32_t px : buf) {
        uint32_t a = px >> 24;
        CHECK(((px >> 16) & 0xff) <= a);
        CHECK(((px >> 8) & 0xff) <= a);
        CHECK((px & 0xff) <= a);
    }
}

TEST_CASE("characters render with visibly different accents", "[avatar]") {
    auto echo = render(AvatarState::Listening, 2.0, 1.0, 0.8f, "echo");
    auto hermes = render(AvatarState::Listening, 2.0, 1.0, 0.8f, "hermes");
    CHECK(echo != hermes);
}

TEST_CASE("renderer respects arbitrary sizes", "[avatar]") {
    for (int size : {64, 96, 192}) {
        auto buf = render(AvatarState::Idle, 1.0, 1.0, 0.f, "echo", size);
        CHECK(painted(buf) > uint64_t(size) * size / 4);
    }
}

TEST_CASE("state labels caption active states only", "[avatar]") {
    CHECK(std::string(avatar_state_label(AvatarState::Listening)) == "LISTENING");
    CHECK(std::string(avatar_state_label(AvatarState::Thinking)) == "THINKING");
    CHECK(std::string(avatar_state_label(AvatarState::Writing)) == "WRITING");
    CHECK(std::string(avatar_state_label(AvatarState::Error)) == "ERROR");
    // Idle and Hidden stay silent — a resting companion is just the mark.
    CHECK(std::string(avatar_state_label(AvatarState::Idle)).empty());
    CHECK(std::string(avatar_state_label(AvatarState::Hidden)).empty());
}

TEST_CASE("the label adds paint in the bottom strip", "[avatar]") {
    // Listening (captioned) must paint more in the bottom rows than Idle
    // (uncaptioned) at the same instant — proves the label renders.
    const int size = 96;
    auto count_bottom = [](AvatarState s) {
        std::vector<uint32_t> buf(size_t(96) * 96, 0);
        avatar_rasterize(buf.data(), 96, *find_character("echo"), s, 0.0, 0.0, 0.f);
        uint64_t sum = 0;
        for (int y = 80; y < 96; y++)
            for (int x = 0; x < 96; x++) sum += buf[y * 96 + x] >> 24;
        return sum;
    };
    CHECK(count_bottom(AvatarState::Listening) > count_bottom(AvatarState::Idle));
}

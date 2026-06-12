#include "avatar/avatar.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace autowhisper {

// MSVC hides the POSIX math constants behind a feature macro; carry our own.
namespace { constexpr double kTau = 6.283185307179586; }

// ---- The pantheon ----------------------------------------------------------
// Echo: repeats your words — the default scribe-spirit. Hermes: the herald,
// quick, message-blue. Mnemosyne: memory, unhurried amber.
const AvatarCharacter AVATAR_CHARACTERS[] = {
    {"echo",      "Echo",      "the one who answers", 0.86f, 0.26f, 0.20f, 1.00f},
    {"hermes",    "Hermes",    "the swift herald",    0.25f, 0.47f, 0.95f, 0.85f},
    {"mnemosyne", "Mnemosyne", "keeper of memory",    0.83f, 0.58f, 0.18f, 1.20f},
};
const int AVATAR_CHARACTER_COUNT =
    sizeof(AVATAR_CHARACTERS) / sizeof(AVATAR_CHARACTERS[0]);

const AvatarCharacter* find_character(const std::string& id) {
    for (int i = 0; i < AVATAR_CHARACTER_COUNT; i++) {
        if (id == AVATAR_CHARACTERS[i].id) return &AVATAR_CHARACTERS[i];
    }
    return &AVATAR_CHARACTERS[0];
}

// ---- State machine ---------------------------------------------------------

void AvatarStateMachine::request(AvatarState s, double now) {
    requested_ = s;
    if (s == AvatarState::Summoned) summoned_until_ = now + 0.45;
    if (s == AvatarState::Writing) writing_until_ = now + 1.4;
}

AvatarState AvatarStateMachine::effective(double now) const {
    AvatarState out = requested_;
    if (requested_ == AvatarState::Summoned && now >= summoned_until_) {
        out = AvatarState::Listening;
    }
    // Writing latches so a few-ms injection still reads as "she wrote it".
    if (writing_until_ > now &&
        (requested_ == AvatarState::Idle || requested_ == AvatarState::Writing ||
         requested_ == AvatarState::Ambient)) {
        out = AvatarState::Writing;
    }
    if (out != shown_) {
        shown_ = out;
        shown_since_ = now;
    }
    return shown_;
}

double AvatarStateMachine::phase(double now) const {
    return std::max(0.0, now - shown_since_);
}

// ---- Software renderer -----------------------------------------------------
// Premultiplied BGRA. Primitives are signed-distance shapes with a 1.2px
// feather; everything is resolution-independent off `size`.

namespace {

struct Col { float r, g, b; };

inline void blend(uint32_t* buf, int size, int x, int y, Col c, float a) {
    if (a <= 0.f || x < 0 || y < 0 || x >= size || y >= size) return;
    a = std::min(a, 1.f);
    uint32_t& px = buf[y * size + x];
    float db = (px & 0xff) / 255.f;
    float dg = ((px >> 8) & 0xff) / 255.f;
    float dr = ((px >> 16) & 0xff) / 255.f;
    float da = ((px >> 24) & 0xff) / 255.f;
    // src-over with premultiplied source
    float sb = c.b * a, sg = c.g * a, sr = c.r * a;
    float ob = sb + db * (1.f - a);
    float og = sg + dg * (1.f - a);
    float orr = sr + dr * (1.f - a);
    float oa = a + da * (1.f - a);
    px = (uint32_t(oa * 255.f + .5f) << 24) | (uint32_t(orr * 255.f + .5f) << 16) |
         (uint32_t(og * 255.f + .5f) << 8) | uint32_t(ob * 255.f + .5f);
}

// alpha from a signed distance (negative = inside), feathered edge
inline float cov(float d, float feather = 1.2f) {
    return std::clamp(0.5f - d / feather, 0.f, 1.f);
}

void disc(uint32_t* buf, int size, float cx, float cy, float r, Col c, float a) {
    int x0 = std::max(0, int(cx - r - 2)), x1 = std::min(size - 1, int(cx + r + 2));
    int y0 = std::max(0, int(cy - r - 2)), y1 = std::min(size - 1, int(cy + r + 2));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float d = std::hypot(x + .5f - cx, y + .5f - cy) - r;
            blend(buf, size, x, y, c, a * cov(d));
        }
}

void ring(uint32_t* buf, int size, float cx, float cy, float r, float w, Col c, float a) {
    float ro = r + w * .5f;
    int x0 = std::max(0, int(cx - ro - 2)), x1 = std::min(size - 1, int(cx + ro + 2));
    int y0 = std::max(0, int(cy - ro - 2)), y1 = std::min(size - 1, int(cy + ro + 2));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float d = std::fabs(std::hypot(x + .5f - cx, y + .5f - cy) - r) - w * .5f;
            blend(buf, size, x, y, c, a * cov(d));
        }
}

// soft radial glow, alpha falls off quadratically to `radius`
void glow(uint32_t* buf, int size, float cx, float cy, float radius, Col c, float a) {
    int x0 = std::max(0, int(cx - radius)), x1 = std::min(size - 1, int(cx + radius));
    int y0 = std::max(0, int(cy - radius)), y1 = std::min(size - 1, int(cy + radius));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float d = std::hypot(x + .5f - cx, y + .5f - cy) / radius;
            if (d >= 1.f) continue;
            float fall = (1.f - d);
            blend(buf, size, x, y, c, a * fall * fall);
        }
}

// rounded stroke from (x0,y0) to (x1,y1)
void capsule(uint32_t* buf, int size, float ax, float ay, float bx, float by,
             float w, Col c, float a) {
    float minx = std::min(ax, bx) - w - 2, maxx = std::max(ax, bx) + w + 2;
    float miny = std::min(ay, by) - w - 2, maxy = std::max(ay, by) + w + 2;
    int x0 = std::max(0, int(minx)), x1 = std::min(size - 1, int(maxx));
    int y0 = std::max(0, int(miny)), y1 = std::min(size - 1, int(maxy));
    float vx = bx - ax, vy = by - ay;
    float len2 = std::max(vx * vx + vy * vy, 1e-6f);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float px = x + .5f - ax, py = y + .5f - ay;
            float t = std::clamp((px * vx + py * vy) / len2, 0.f, 1.f);
            float d = std::hypot(px - t * vx, py - t * vy) - w * .5f;
            blend(buf, size, x, y, c, a * cov(d));
        }
}

constexpr Col kInk{0.96f, 0.96f, 0.95f};   // luminous core stroke
constexpr Col kRim{0.07f, 0.07f, 0.08f};   // dark rim so it reads on light desktops

// A 5x7 uppercase/space bitmap font, just enough for the state labels below
// the orb (no font dependency, identical on every platform). Each glyph is
// 7 rows of a 5-bit mask, top row first.
struct Glyph { char c; uint8_t rows[7]; };
constexpr Glyph kFont[] = {
    {' ', {0,0,0,0,0,0,0}},
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
    {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
};

const Glyph* find_glyph(char c) {
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    for (const auto& g : kFont) if (g.c == c) return &g;
    return &kFont[0];  // space
}

void draw_label(uint32_t* buf, int size, const char* text, float cx, float cy,
                float px, Col c, float a) {
    int len = 0;
    for (const char* p = text; *p; ++p) len++;
    if (len == 0) return;
    const float gw = 5 * px, gap = px * 1.6f, advance = gw + gap;
    float total = len * advance - gap;
    float x = cx - total * .5f;
    for (const char* p = text; *p; ++p) {
        const Glyph* g = find_glyph(*p);
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (g->rows[row] & (1 << (4 - col))) {
                    // Square pixels read crisper than discs at caption size.
                    int gx = int(x + col * px), gy = int(cy + row * px);
                    int gpx = std::max(1, int(px + 0.5f));
                    for (int yy = 0; yy < gpx; yy++)
                        for (int xx = 0; xx < gpx; xx++)
                            blend(buf, size, gx + xx, gy + yy, c, a);
                }
            }
        }
        x += advance;
    }
}

} // namespace

const char* avatar_state_label(AvatarState s) {
    switch (s) {
        case AvatarState::Summoned:  return "READY";
        case AvatarState::Listening: return "LISTENING";
        case AvatarState::Thinking:  return "THINKING";
        case AvatarState::Writing:   return "WRITING";
        case AvatarState::Ambient:   return "IDLE";
        case AvatarState::Error:     return "ERROR";
        default:                     return "";
    }
}

void avatar_rasterize(uint32_t* buf, int size, const AvatarCharacter& ch,
                      AvatarState state, double t, double phase, float level) {
    std::memset(buf, 0, size_t(size) * size * 4);
    if (state == AvatarState::Hidden) return;

    const float S = float(size);
    const float cx = S * .5f;
    const Col accent{ch.accent_r, ch.accent_g, ch.accent_b};
    const double tt = t / ch.tempo;
    level = std::clamp(level, 0.f, 1.f);

    // The spirit floats: a slow vertical bob (skipped while writing — a
    // scribe holds still).
    float bob = (state == AvatarState::Writing)
                    ? 0.f
                    : float(std::sin(tt * kTau / 3.2)) * S * .022f;
    // Writing shifts the orb up to make room for the written line.
    float cy = S * (state == AvatarState::Writing ? .40f : .46f) + bob;
    const float R = S * .175f;       // core ring radius
    const float W = S * .032f;       // core stroke width

    // --- aura ---
    switch (state) {
        case AvatarState::Idle: {
            float br = .10f + .04f * float(std::sin(tt * kTau / 3.2));
            glow(buf, size, cx, cy, S * .34f, kInk, br);
            break;
        }
        case AvatarState::Summoned: {
            float p = std::clamp(float(phase / 0.45), 0.f, 1.f);
            glow(buf, size, cx, cy, S * (.30f + .16f * p), accent, .15f + .55f * p);
            ring(buf, size, cx, cy, R + (S * .30f) * p, W * .6f, accent, .5f * (1.f - p));
            break;
        }
        case AvatarState::Listening: {
            glow(buf, size, cx, cy, S * (.34f + .10f * level), accent, .30f + .50f * level);
            // halo breathes with the live level
            ring(buf, size, cx, cy, R * (1.45f + .55f * level), W * .55f, accent,
                 .25f + .45f * level);
            break;
        }
        case AvatarState::Thinking: {
            float br = .22f + .08f * float(std::sin(tt * kTau / 1.6));
            glow(buf, size, cx, cy, S * .32f, accent, br);
            break;
        }
        case AvatarState::Writing:
            glow(buf, size, cx, cy, S * .28f, accent, .20f);
            break;
        case AvatarState::Ambient: {
            // shimmer between ink and accent while something else plays
            float m = .5f + .5f * float(std::sin(tt * kTau / 5.0));
            Col mix{kInk.r + (accent.r - kInk.r) * m * .6f,
                    kInk.g + (accent.g - kInk.g) * m * .6f,
                    kInk.b + (accent.b - kInk.b) * m * .6f};
            glow(buf, size, cx, cy, S * .33f, mix, .16f);
            break;
        }
        case AvatarState::Error: {
            float blink = std::fmod(phase, 0.5) < 0.3 ? .55f : .15f;
            glow(buf, size, cx, cy, S * .33f, Col{.83f, .58f, .18f}, blink);
            break;
        }
        default: break;
    }

    // --- the mark: ring + the two side dashes (the AutoWhisper sigil) ---
    ring(buf, size, cx, cy, R, W * 1.9f, kRim, .55f);
    ring(buf, size, cx, cy, R, W, kInk, .95f);
    float dash = S * .052f, gap = R + S * .045f;
    float dw = W * 1.05f;
    capsule(buf, size, cx - gap - dash, cy, cx - gap, cy, dw * 2.2f, kRim, .45f);
    capsule(buf, size, cx + gap, cy, cx + gap + dash, cy, dw * 2.2f, kRim, .45f);
    capsule(buf, size, cx - gap - dash, cy, cx - gap, cy, dw, kInk, .95f);
    capsule(buf, size, cx + gap, cy, cx + gap + dash, cy, dw, kInk, .95f);

    // --- per-state actors ---
    if (state == AvatarState::Listening) {
        // inner mote rises with voice level
        disc(buf, size, cx, cy, R * (.30f + .25f * level), accent, .85f);
    } else if (state == AvatarState::Thinking) {
        double ang = tt * kTau / 1.1;
        float ox = cx + std::cos(float(ang)) * (R * 1.55f);
        float oy = cy + std::sin(float(ang)) * (R * 1.55f);
        disc(buf, size, ox, oy, S * .030f, accent, .9f);
        disc(buf, size, cx, cy, R * .28f, kInk, .35f);
    } else if (state == AvatarState::Writing) {
        // She writes: three strokes appear left to right, then the line
        // settles; a bright nib leads the active stroke.
        float seg[3] = {.30f, .22f, .34f};   // widths as a fraction of line
        float lineW = S * .58f, x0 = cx - lineW * .5f;
        float yline = S * .78f;
        double cycle = std::fmod(phase, 1.6);
        for (int i = 0; i < 3; i++) {
            double begin = 0.12 + i * 0.38;
            float p = std::clamp(float((cycle - begin) / 0.30), 0.f, 1.f);
            if (p <= 0.f) continue;
            float sx = x0 + lineW * (i == 0 ? 0.f : seg[0] + (i == 1 ? .08f : seg[1] + .16f));
            float ex = sx + lineW * seg[i] * p;
            capsule(buf, size, sx, yline, ex, yline, S * .028f, kInk, .9f);
            if (p < 1.f) disc(buf, size, ex, yline, S * .026f, accent, .95f);
        }
    } else if (state == AvatarState::Ambient) {
        // two soft notes drifting beside the orb
        float dy = float(std::sin(tt * kTau / 2.6)) * S * .02f;
        disc(buf, size, cx - R * 2.1f, cy + dy, S * .018f, kInk, .35f);
        disc(buf, size, cx + R * 2.1f, cy - dy, S * .018f, kInk, .35f);
    }

    // --- state label (the floating-button caption, Wispr-Flow style) ---
    // Active states get a short word in the bottom strip; idle stays silent
    // so a resting companion is just the mark.
    const char* label = avatar_state_label(state);
    if (label[0] != '\0') {
        float lx = S * .013f;                     // ~5px glyph pixel at 96px
        float ly = S * (state == AvatarState::Writing ? .90f : .88f);
        // soft plate so the caption reads on any wallpaper
        int chars = 0; for (const char* p = label; *p; ++p) chars++;
        float platew = chars * lx * 6 + S * .06f;
        for (int y = int(ly - lx * 1.4f); y < int(ly + lx * 8.4f); y++)
            for (int x = int(cx - platew * .5f); x < int(cx + platew * .5f); x++)
                blend(buf, size, x, y, kRim, .28f);
        draw_label(buf, size, label, cx, ly, lx, kInk, .92f);
    }
}

} // namespace autowhisper

// ---- Ticker ----------------------------------------------------------------

#include <chrono>

namespace autowhisper {

namespace {
double mono_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

AvatarTicker::AvatarTicker(AvatarStateMachine machine, std::function<float()> level,
                           std::function<bool()> ambient)
    : machine_(machine), level_(std::move(level)), ambient_(std::move(ambient)) {}

void AvatarTicker::request(AvatarState s) {
    requested_.store(int(s), std::memory_order_relaxed);
    dirty_.store(true, std::memory_order_release);
}

AvatarTicker::Snapshot AvatarTicker::tick() {
    const double now = mono_seconds();
    if (start_time_ < 0) start_time_ = now;

    if (dirty_.exchange(false, std::memory_order_acq_rel)) {
        machine_.request(AvatarState(requested_.load(std::memory_order_relaxed)), now);
    }
    AvatarState s = machine_.effective(now);

    // While idle, glance every ~3s at whether something else is playing.
    if (s == AvatarState::Idle && ambient_) {
        if (--ambient_countdown_ <= 0) {
            ambient_countdown_ = 90;
            ambient_active_ = ambient_();
        }
        if (ambient_active_) s = AvatarState::Ambient;
    } else {
        ambient_countdown_ = 0;
        ambient_active_ = false;
    }

    float lvl = 0.f;
    if (s == AvatarState::Listening && level_) lvl = level_();

    return Snapshot{s, now - start_time_, machine_.phase(now), lvl};
}

}  // namespace autowhisper

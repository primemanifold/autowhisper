#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace autowhisper {

inline constexpr std::uint32_t RUNTIME_SAMPLE_RATE = 16000;
inline constexpr std::int64_t MAX_TRANSCRIPTION_AUDIO_SECONDS = 3600;
inline constexpr std::uintmax_t MAX_TRANSCRIPTION_FILE_BYTES = 512ULL * 1024ULL * 1024ULL;

struct DecodedAudio {
    std::vector<float> samples;
    std::uint32_t sample_rate = RUNTIME_SAMPLE_RATE;

    std::int64_t duration_ms() const;
};

DecodedAudio decode_audio_file(const std::string& path);

} // namespace autowhisper

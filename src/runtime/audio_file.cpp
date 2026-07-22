#include "runtime/audio_file.h"

#include <miniaudio.h>

#include <array>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

namespace autowhisper {

namespace {

class DecoderGuard {
public:
    explicit DecoderGuard(ma_decoder* decoder) : decoder_(decoder) {}
    ~DecoderGuard() { ma_decoder_uninit(decoder_); }

    DecoderGuard(const DecoderGuard&) = delete;
    DecoderGuard& operator=(const DecoderGuard&) = delete;

private:
    ma_decoder* decoder_;
};

} // namespace

std::int64_t DecodedAudio::duration_ms() const {
    if (sample_rate == 0) return 0;
    const auto sample_count = static_cast<std::int64_t>(samples.size());
    return (sample_count * 1000) / static_cast<std::int64_t>(sample_rate);
}

DecodedAudio decode_audio_file(const std::string& path) {
    if (path.empty()) throw std::invalid_argument("Audio path is empty");

    std::error_code ec;
    const fs::path input(path);
    if (!fs::is_regular_file(input, ec) || ec) {
        throw std::runtime_error("Audio file does not exist or is not a regular file: " + path);
    }
    const auto size = fs::file_size(input, ec);
    if (ec) throw std::runtime_error("Cannot inspect audio file: " + path);
    if (size == 0) throw std::runtime_error("Audio file is empty: " + path);
    if (size > MAX_TRANSCRIPTION_FILE_BYTES) {
        throw std::runtime_error("Audio file exceeds the 512 MiB limit");
    }

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, RUNTIME_SAMPLE_RATE);
    ma_decoder decoder{};
    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
        throw std::runtime_error("Cannot decode audio file: " + path);
    }
    DecoderGuard guard(&decoder);

    constexpr ma_uint64 CHUNK_FRAMES = 4096;
    constexpr std::size_t MAX_SAMPLES =
        static_cast<std::size_t>(MAX_TRANSCRIPTION_AUDIO_SECONDS) * RUNTIME_SAMPLE_RATE;
    std::array<float, CHUNK_FRAMES> chunk{};
    DecodedAudio audio;
    audio.samples.reserve(RUNTIME_SAMPLE_RATE * 30);

    for (;;) {
        ma_uint64 frames_read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(
            &decoder,
            chunk.data(),
            CHUNK_FRAMES,
            &frames_read
        );
        if (frames_read > 0) {
            if (audio.samples.size() > MAX_SAMPLES - static_cast<std::size_t>(frames_read)) {
                throw std::runtime_error("Decoded audio exceeds the 60 minute limit");
            }
            audio.samples.insert(
                audio.samples.end(),
                chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(frames_read)
            );
        }
        if (result == MA_AT_END) break;
        if (result != MA_SUCCESS) {
            throw std::runtime_error("Audio decoder failed while reading: " + path);
        }
        if (frames_read == 0) break;
    }

    if (audio.samples.empty()) {
        throw std::runtime_error("Decoded zero audio samples from: " + path);
    }
    return audio;
}

} // namespace autowhisper

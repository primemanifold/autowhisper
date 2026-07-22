#include <catch2/catch_test_macros.hpp>

#include "runtime/audio_file.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

using namespace autowhisper;

namespace {

class TemporaryFile {
public:
    explicit TemporaryFile(std::string suffix) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("autowhisper-audio-test-" + std::to_string(stamp) + std::move(suffix));
    }
    ~TemporaryFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void write_u16(std::ostream& output, std::uint16_t value) {
    const std::array<char, 2> bytes{
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_silent_pcm_wav(const std::filesystem::path& path, std::uint32_t frames) {
    constexpr std::uint16_t channels = 1;
    constexpr std::uint16_t bits_per_sample = 16;
    constexpr std::uint32_t bytes_per_sample = bits_per_sample / 8;
    const std::uint32_t data_bytes = frames * channels * bytes_per_sample;

    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.good());
    output.write("RIFF", 4);
    write_u32(output, 36 + data_bytes);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    write_u32(output, 16);
    write_u16(output, 1);
    write_u16(output, channels);
    write_u32(output, RUNTIME_SAMPLE_RATE);
    write_u32(output, RUNTIME_SAMPLE_RATE * channels * bytes_per_sample);
    write_u16(output, channels * bytes_per_sample);
    write_u16(output, bits_per_sample);
    output.write("data", 4);
    write_u32(output, data_bytes);
    for (std::uint32_t index = 0; index < frames; ++index) write_u16(output, 0);
}

} // namespace

TEST_CASE("Audio file decoder produces 16 kHz mono float samples", "[runtime][audio]") {
    TemporaryFile file(".wav");
    write_silent_pcm_wav(file.path(), 1600);

    const auto audio = decode_audio_file(file.path().string());
    CHECK(audio.sample_rate == 16000);
    CHECK(audio.samples.size() == 1600);
    CHECK(audio.duration_ms() == 100);
}

TEST_CASE("Audio file decoder rejects missing and empty files", "[runtime][audio]") {
    TemporaryFile missing(".wav");
    CHECK_THROWS(decode_audio_file(missing.path().string()));

    TemporaryFile empty(".wav");
    std::ofstream(empty.path(), std::ios::binary);
    CHECK_THROWS(decode_audio_file(empty.path().string()));
}

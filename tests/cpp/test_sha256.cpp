#include <catch2/catch_test_macros.hpp>

#include "models/models.h"
#include "util/sha256.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace autowhisper;
namespace fs = std::filesystem;

namespace {

std::string write_temp_file(const std::string& content) {
    auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = fs::temp_directory_path() /
                ("autowhisper-sha256-test-" + std::to_string(tick) + ".bin");
    std::ofstream out(path, std::ios::binary);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    return path.string();
}

} // namespace

// Reference digests from FIPS 180-4 / NIST CAVP test vectors.
TEST_CASE("sha256_hex matches known vectors", "[sha256]") {
    CHECK(sha256_hex("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_hex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("sha256_hex handles block boundaries", "[sha256]") {
    // 55, 56, 64, and 119 bytes straddle the padding edge cases.
    CHECK(sha256_hex(std::string(55, 'a')) ==
          "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(sha256_hex(std::string(56, 'a')) ==
          "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(sha256_hex(std::string(64, 'a')) ==
          "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    CHECK(sha256_hex(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("incremental update equals one-shot", "[sha256]") {
    std::string data(100000, 'x');
    Sha256 h;
    size_t off = 0;
    size_t chunks[] = {1, 63, 64, 65, 1000, 99999};
    for (size_t c : chunks) {
        size_t n = std::min(c, data.size() - off);
        h.update(data.data() + off, n);
        off += n;
        if (off >= data.size()) break;
    }
    if (off < data.size()) h.update(data.data() + off, data.size() - off);
    CHECK(h.hex_digest() == sha256_hex(data));
}

TEST_CASE("sha256_file_hex hashes file contents", "[sha256]") {
    std::string path = write_temp_file("abc");
    CHECK(sha256_file_hex(path) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    fs::remove(path);
}

TEST_CASE("sha256_file_hex returns empty for missing file", "[sha256]") {
    CHECK(sha256_file_hex("/nonexistent/path/autowhisper-test.bin").empty());
}

TEST_CASE("verify_model_checksum accepts matching file", "[sha256][models]") {
    std::string path = write_temp_file("abc");
    ModelInfo info{"test", "d", "s", "sp", "ggml-test.bin", "https://example.com",
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", true};
    CHECK(verify_model_checksum(info, path));
    fs::remove(path);
}

TEST_CASE("verify_model_checksum rejects mismatched file", "[sha256][models]") {
    std::string path = write_temp_file("tampered contents");
    ModelInfo info{"test", "d", "s", "sp", "ggml-test.bin", "https://example.com",
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", true};
    CHECK_FALSE(verify_model_checksum(info, path));
    fs::remove(path);
}

TEST_CASE("verify_model_checksum rejects unreadable file", "[sha256][models]") {
    ModelInfo info{"test", "d", "s", "sp", "ggml-test.bin", "https://example.com",
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", true};
    CHECK_FALSE(verify_model_checksum(info, "/nonexistent/path/model.bin"));
}

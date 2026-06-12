#pragma once

#include <cstdint>
#include <string>

namespace autowhisper {

// Incremental SHA-256 (FIPS 180-4). Dependency-free so it works identically
// on Linux, macOS, and Windows builds.
class Sha256 {
public:
    Sha256();

    void update(const void* data, size_t len);

    // Finalizes and returns the digest as a 64-char lowercase hex string.
    // The object must not be reused after calling.
    std::string hex_digest();

private:
    void process_block(const uint8_t* block);

    uint32_t state_[8];
    uint64_t total_len_ = 0;
    uint8_t buffer_[64];
    size_t buffer_len_ = 0;
};

// One-shot helpers.
std::string sha256_hex(const std::string& data);

// Hashes a file in streaming chunks. Returns empty string if the file
// cannot be opened or read.
std::string sha256_file_hex(const std::string& path);

} // namespace autowhisper

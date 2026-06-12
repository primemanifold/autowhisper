#include "models/models.h"
#include "util/sha256.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace autowhisper {

bool verify_model_checksum(const ModelInfo& info, const std::string& path) {
    if (!info.sha256 || std::string(info.sha256).empty()) {
        // No pinned checksum for this entry; treat as unverifiable-but-allowed.
        return true;
    }
    std::string actual = sha256_file_hex(path);
    if (actual.empty()) {
        spdlog::warn("Could not read {} for checksum verification", path);
        return false;
    }
    if (actual != info.sha256) {
        spdlog::warn("Checksum mismatch for {}: expected {}, got {}", path, info.sha256, actual);
        return false;
    }
    return true;
}

// Standard models from ggerganov/whisper.cpp, distil models from distil-whisper repos.
// sha256 values are the Hugging Face LFS oids of each file (verified 2026-06-12).
// Speeds marked "—" are unmeasured; the bench harness is the source of truth.
const ModelInfo MODELS[] = {
    {"tiny.en",         "Fastest, good accuracy",       "~78MB",  "78ms",  "ggml-tiny.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin",
     "921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f", true},
    {"tiny",            "Fastest, multilingual",        "~78MB",  "—",     "ggml-tiny.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin",
     "be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21", false},
    {"base.en",         "Fast, better accuracy",        "~148MB", "143ms", "ggml-base.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin",
     "a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002", true},
    {"base",            "Fast, multilingual",           "~148MB", "—",     "ggml-base.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
     "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe", false},
    {"small.en",        "Balanced",                     "~488MB", "211ms", "ggml-small.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin",
     "c6138d6d58ecc8322097e0f987c32f1be8bb0a18532a3f88f734d1bbf9c41e5d", true},
    {"small",           "Balanced, multilingual",       "~488MB", "—",     "ggml-small.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin",
     "1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b", false},
    {"distil-small.en", "Optimized small (recommended)","~336MB", "198ms", "ggml-distil-small.en.bin",
     "https://huggingface.co/distil-whisper/distil-small.en/resolve/main/ggml-distil-small.en.bin",
     "7691eb11167ab7aaf6b3e05d8266f2fd9ad89c550e433f86ac266ebdee6c970a", true},
    {"medium.en",       "Medium accuracy",              "~1.5GB", "381ms", "ggml-medium.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin",
     "cc37e93478338ec7700281a7ac30a10128929eb8f427dda2e865faa8f6da4356", true},
    {"medium",          "Medium accuracy, multilingual","~1.5GB", "—",     "ggml-medium.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin",
     "6c14d5adee5f86394037b4e4e8b59f1673b6cee10e3cf0b11bbdbee79c156208", false},
    {"distil-medium.en","Optimized medium",             "~794MB", "381ms", "ggml-medium-32-2.en.bin",
     "https://huggingface.co/distil-whisper/distil-medium.en/resolve/main/ggml-medium-32-2.en.bin",
     "ad53ccb618188b210550e98cc32bf5a13188d86635e395bb11115ed275d6e7aa", true},
    {"distil-large-v3", "Best English accuracy",        "~1.5GB", "448ms", "ggml-distil-large-v3.bin",
     "https://huggingface.co/distil-whisper/distil-large-v3-ggml/resolve/main/ggml-distil-large-v3.bin",
     "2883a11b90fb10ed592d826edeaee7d2929bf1ab985109fe9e1e7b4d2b69a298", true},
    {"large-v3-turbo",  "Best multilingual speed/accuracy", "~1.6GB", "—", "ggml-large-v3-turbo.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo.bin",
     "1fc70f774d38eb169993ac391eea357ef47c88757ef72ee5943879b7e8e2bc69", false},
    {"large-v3",        "Maximum accuracy, multilingual","~3.1GB", "926ms", "ggml-large-v3.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin",
     "64d182b440b98d5203c4f9bd541544d84c605196c4f7b845dfa11fb23594d1e2", false},
};

const int MODEL_COUNT = sizeof(MODELS) / sizeof(MODELS[0]);

std::string get_cache_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.cache/whisper";
}

const ModelInfo* find_model(const std::string& name) {
    for (int i = 0; i < MODEL_COUNT; i++) {
        if (MODELS[i].name == name) return &MODELS[i];
    }
    return nullptr;
}

bool model_is_english_only(const std::string& name) {
    if (const ModelInfo* info = find_model(name)) return info->english_only;
    // Convention fallback for catalog-external models: whisper English-only
    // models end in ".en", and all Distil-Whisper models are English-only.
    if (name.size() > 3 && name.compare(name.size() - 3, 3, ".en") == 0) return true;
    return name.rfind("distil-", 0) == 0;
}

std::string get_model_path(const std::string& name) {
    auto* info = find_model(name);
    if (!info) return "";

    std::string cache = get_cache_dir();
    return cache + "/" + info->ggml_file;
}

bool is_model_downloaded(const std::string& name) {
    std::string path = get_model_path(name);
    return !path.empty() && fs::exists(path);
}

void list_models() {
    std::cout << "\n\033[1mAvailable Models\033[0m\n";
    std::cout << std::string(60, '-') << "\n\n";

    for (int i = 0; i < MODEL_COUNT; i++) {
        const auto& m = MODELS[i];
        bool downloaded = is_model_downloaded(m.name);

        std::string status = downloaded ? "\033[32m\xe2\x9c\x93\033[0m" : " ";
        std::string recommended = std::string(m.name) == "distil-small.en" ? " (recommended)" : "";

        std::cout << "  " << status << " \033[1m" << m.name << "\033[0m" << recommended << "\n";
        std::cout << "      " << m.description << "\n";
        std::cout << "      Size: " << m.size << " | Speed: " << m.speed
                  << " | Languages: " << (m.english_only ? "English" : "100+ (auto-detect)")
                  << "\n\n";
    }

    std::cout << std::string(60, '-') << "\n";
    std::cout << "  \xe2\x9c\x93 = downloaded\n\n";
    std::cout << "Download a model: autowhisper model download <name>\n\n";
}

int download_model(const std::string& name) {
    auto* info = find_model(name);
    if (!info) {
        std::cerr << "\033[31mUnknown model: " << name << "\033[0m\n";
        std::cerr << "Available models: ";
        for (int i = 0; i < MODEL_COUNT; i++) {
            if (i > 0) std::cerr << ", ";
            std::cerr << MODELS[i].name;
        }
        std::cerr << "\n";
        return 1;
    }

    std::cout << "\n\033[1mDownloading " << name << "\033[0m\n";
    std::cout << "  " << info->description << "\n";
    std::cout << "  Size: " << info->size << "\n\n";

    // Create cache directory
    std::string cache_dir = get_cache_dir();
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) {
        std::cerr << "\033[31mFailed to create cache directory: " << cache_dir
                  << " (" << ec.message() << ")\033[0m\n";
        return 1;
    }

    std::string dest = cache_dir + "/" + info->ggml_file;

    if (fs::exists(dest)) {
        if (verify_model_checksum(*info, dest)) {
            std::cout << "\033[32m\xe2\x9c\x93 Model " << name << " already downloaded!\033[0m\n\n";
            return 0;
        }
        // Cached file is corrupt or stale; re-download.
        std::cout << "\033[33mCached file failed checksum verification; re-downloading\033[0m\n";
        std::error_code rm_ec;
        fs::remove(dest, rm_ec);
        if (rm_ec) {
            std::cerr << "\033[31mFailed to remove corrupt file: " << dest
                      << " (" << rm_ec.message() << ")\033[0m\n";
            return 1;
        }
    }

    std::string url = info->url;

    std::cout << "Downloading from: " << url << "\n";

    // Use curl or wget
    int rc;
    if (command_exists("curl")) {
        rc = run_passthrough({"curl", "-L", "-o", dest, "--progress-bar", url});
    } else if (command_exists("wget")) {
        rc = run_passthrough({"wget", "-O", dest, "--show-progress", url});
    } else {
        std::cerr << "\033[31mNeither curl nor wget available for download\033[0m\n";
        return 1;
    }

    std::error_code stat_ec;
    const bool is_valid_file = fs::is_regular_file(dest, stat_ec) &&
                               fs::file_size(dest, stat_ec) > 1000;
    if (rc == 0 && !stat_ec && is_valid_file) {
        std::cout << "Verifying checksum...\n";
        if (verify_model_checksum(*info, dest)) {
            std::cout << "\n\033[32m\xe2\x9c\x93 Model " << name << " ready!\033[0m\n\n";
            return 0;
        }
        std::error_code rm_ec;
        fs::remove(dest, rm_ec);
        std::cerr << "\033[31mChecksum verification failed; the download was corrupt "
                     "or tampered with. The file has been removed — please retry.\033[0m\n";
        return 1;
    }

    // Clean up partial download
    std::error_code rm_ec;
    fs::remove(dest, rm_ec);
    if (stat_ec) {
        spdlog::warn("Failed to inspect downloaded model {}: {}", dest, stat_ec.message());
    }
    std::cerr << "\033[31mDownload failed\033[0m\n";
    return 1;

}

} // namespace autowhisper

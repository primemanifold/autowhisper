#include "models/models.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace autowhisper {

// Standard models from ggerganov/whisper.cpp, distil models from distil-whisper repos
const ModelInfo MODELS[] = {
    {"tiny.en",         "Fastest, good accuracy",       "~75MB",  "78ms",  "ggml-tiny.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin"},
    {"base.en",         "Fast, better accuracy",        "~150MB", "143ms", "ggml-base.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin"},
    {"small.en",        "Balanced",                     "~500MB", "211ms", "ggml-small.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin"},
    {"distil-small.en", "Optimized small (recommended)","~166MB", "198ms", "ggml-distil-small.en.bin",
     "https://huggingface.co/distil-whisper/distil-small.en/resolve/main/ggml-distil-small.en.bin"},
    {"medium.en",       "Medium accuracy",              "~1.5GB", "381ms", "ggml-medium.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin"},
    {"distil-medium.en","Optimized medium",             "~800MB", "381ms", "ggml-distil-medium.en.bin",
     "https://huggingface.co/distil-whisper/distil-medium.en/resolve/main/ggml-distil-medium.en.bin"},
    {"distil-large-v3", "Best accuracy",                "~1.5GB", "448ms", "ggml-distil-large-v3.bin",
     "https://huggingface.co/distil-whisper/distil-large-v3-ggml/resolve/main/ggml-distil-large-v3.bin"},
    {"large-v3",        "Maximum accuracy",             "~3GB",   "926ms", "ggml-large-v3.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin"},
};

const int MODEL_COUNT = sizeof(MODELS) / sizeof(MODELS[0]);

static std::string get_cache_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.cache/whisper";
}

static const ModelInfo* find_model(const std::string& name) {
    for (int i = 0; i < MODEL_COUNT; i++) {
        if (MODELS[i].name == name) return &MODELS[i];
    }
    return nullptr;
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
        std::cout << "      Size: " << m.size << " | Speed: " << m.speed << "\n\n";
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
    fs::create_directories(cache_dir);

    std::string dest = cache_dir + "/" + info->ggml_file;

    if (fs::exists(dest)) {
        std::cout << "\033[32m\xe2\x9c\x93 Model " << name << " already downloaded!\033[0m\n\n";
        return 0;
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

    if (rc == 0 && fs::exists(dest) && fs::file_size(dest) > 1000) {
        std::cout << "\n\033[32m\xe2\x9c\x93 Model " << name << " ready!\033[0m\n\n";
        return 0;
    } else {
        // Clean up partial download
        if (fs::exists(dest)) fs::remove(dest);
        std::cerr << "\033[31mDownload failed\033[0m\n";
        return 1;
    }
}

} // namespace autowhisper

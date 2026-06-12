#pragma once

#include <string>

namespace autowhisper {

struct ModelInfo {
    const char* name;
    const char* description;
    const char* size;
    const char* speed;
    const char* ggml_file;  // GGML filename
    const char* url;         // Full download URL
    const char* sha256;      // Expected SHA-256 of the downloaded file (lowercase hex)
    bool english_only;       // false = multilingual (supports language="auto")
};

// Available models
extern const ModelInfo MODELS[];
extern const int MODEL_COUNT;

bool is_model_downloaded(const std::string& name);
void list_models();
int download_model(const std::string& name);

// Get the GGML model path for a given name
std::string get_model_path(const std::string& name);

// Model lookup and cache directory helpers
const ModelInfo* find_model(const std::string& name);
std::string get_cache_dir();

// Returns true when the file at `path` matches the catalog checksum for
// `info` (or when the entry has no pinned checksum).
bool verify_model_checksum(const ModelInfo& info, const std::string& path);

// True when `name` only supports English (catalog lookup, with a ".en"
// suffix-convention fallback for models outside the catalog).
bool model_is_english_only(const std::string& name);

} // namespace autowhisper

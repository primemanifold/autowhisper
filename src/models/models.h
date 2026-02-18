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

} // namespace autowhisper

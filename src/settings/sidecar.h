#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace autowhisper::settings {

uint64_t fnv64(std::string_view bytes);

std::string to_hex16(uint64_t v);

std::string weak_canonical(const std::string& path);

std::string sidecar_path_for(const std::string& canonical_config_path);

struct SidecarContents {
    int pid = 0;
    int port = 0;
    std::string canonical_path;
};

std::optional<SidecarContents> parse_sidecar(std::string_view raw);

std::string format_sidecar(const SidecarContents& c);

}  // namespace autowhisper::settings

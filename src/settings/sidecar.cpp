#include "settings/sidecar.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace autowhisper::settings {

uint64_t fnv64(std::string_view bytes) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

std::string to_hex16(uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016lx", static_cast<unsigned long>(v));
    return std::string(buf);
}

std::string weak_canonical(const std::string& path) {
    std::error_code ec;
    auto p = fs::weakly_canonical(fs::path(path), ec);
    if (ec) return path;
    return p.string();
}

static bool is_dir_writable(const std::string& dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) return false;
    return access(dir.c_str(), W_OK) == 0;
}

std::string sidecar_path_for(const std::string& canonical_config_path) {
    const std::string hash = to_hex16(fnv64(canonical_config_path));
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] && is_dir_writable(xdg)) {
        return std::string(xdg) + "/autowhisper-settings-" + hash + ".info";
    }
    return "/tmp/autowhisper-settings-" + std::to_string(::getuid()) +
           "-" + hash + ".info";
}

std::optional<SidecarContents> parse_sidecar(std::string_view raw) {
    if (raw.empty()) return std::nullopt;
    std::string s(raw);
    std::istringstream is(s);
    SidecarContents c{};
    std::string pid_line, port_line, path_line;
    if (!std::getline(is, pid_line))  return std::nullopt;
    if (!std::getline(is, port_line)) return std::nullopt;
    if (!std::getline(is, path_line)) return std::nullopt;
    try {
        c.pid  = std::stoi(pid_line);
        c.port = std::stoi(port_line);
    } catch (...) {
        return std::nullopt;
    }
    if (c.pid <= 0 || c.port <= 0 || c.port > 65535) return std::nullopt;
    c.canonical_path = path_line;
    return c;
}

std::string format_sidecar(const SidecarContents& c) {
    std::ostringstream os;
    os << c.pid << "\n" << c.port << "\n" << c.canonical_path << "\n";
    return os.str();
}

}  // namespace autowhisper::settings

#include "settings/sidecar.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <process.h>
#include <io.h>
#ifndef W_OK
#define W_OK 2
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#define access _access
#define getpid _getpid
#else
#include <unistd.h>
#endif

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
    std::ostringstream os;
    os << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << v;
    return os.str();
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

static std::string sidecar_user_suffix() {
#if defined(_WIN32)
    const char* user = std::getenv("USERNAME");
    if (user && user[0]) return std::string(user);
    return "windows";
#else
    return std::to_string(::getuid());
#endif
}

std::string sidecar_path_for(const std::string& canonical_config_path) {
    const std::string hash = to_hex16(fnv64(canonical_config_path));
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] && is_dir_writable(xdg)) {
        return std::string(xdg) + "/autowhisper-settings-" + hash + ".info";
    }
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec);
    if (ec || tmp.empty()) tmp = fs::path("/tmp");
    return (tmp / ("autowhisper-settings-" + sidecar_user_suffix() + "-" + hash + ".info")).string();
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
    std::string token_line;
    if (std::getline(is, token_line)) {
        c.token = token_line;
    }
    return c;
}

std::string format_sidecar(const SidecarContents& c) {
    std::ostringstream os;
    os << c.pid << "\n" << c.port << "\n" << c.canonical_path << "\n" << c.token << "\n";
    return os.str();
}

}  // namespace autowhisper::settings

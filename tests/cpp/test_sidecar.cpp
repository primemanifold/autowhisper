#include <catch2/catch_test_macros.hpp>

#include "settings/sidecar.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace autowhisper::settings;

namespace {

fs::path stable_temp_dir() {
    std::error_code ec;
    auto tmp = fs::temp_directory_path(ec);
    if (ec || !fs::is_directory(tmp)) return fs::path("/tmp");
    return tmp;
}

int current_process_id() {
#if defined(_WIN32)
    return _getpid();
#else
    return getpid();
#endif
}

void set_env_var(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

void unset_env_var(const char* key) {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
}

}  // namespace

TEST_CASE("fnv64 is deterministic and non-trivial", "[sidecar]") {
    CHECK(fnv64("abc") == fnv64("abc"));
    CHECK(fnv64("abc") != fnv64("abd"));
    CHECK(fnv64("") == 0xcbf29ce484222325ULL);
}

TEST_CASE("to_hex16 returns exactly 16 chars, lowercase", "[sidecar]") {
    CHECK(to_hex16(0) == "0000000000000000");
    CHECK(to_hex16(0xdeadbeefULL).size() == 16);
    CHECK(to_hex16(0x100000000ULL) == "0000000100000000");
    CHECK(to_hex16(0xfeedfacedeadbeefULL) == "feedfacedeadbeef");
    auto s = to_hex16(0xfeedfaceULL);
    CHECK(s.size() == 16);
    for (char ch : s) {
        bool is_hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        CHECK(is_hex);
    }
}

TEST_CASE("weak_canonical tolerates non-existent suffix", "[sidecar]") {
    auto tmp = stable_temp_dir();
    auto p = (tmp / "autowhisper_nonexistent_dir" / "new.toml").string();
    auto c = weak_canonical(p);
    CHECK(c.find("autowhisper_nonexistent_dir") != std::string::npos);
    CHECK(c.find("new.toml") != std::string::npos);
    CHECK(c.front() == '/');
}

TEST_CASE("sidecar_path_for uses XDG_RUNTIME_DIR when set", "[sidecar]") {
    const char* saved = std::getenv("XDG_RUNTIME_DIR");
    auto tmp = stable_temp_dir() /
               ("autowhisper_xdg_" + std::to_string(current_process_id()));
    fs::create_directories(tmp);
    set_env_var("XDG_RUNTIME_DIR", tmp.string());

    auto p = sidecar_path_for("/home/isura/.config/autowhisper/config.toml");
    CHECK(p.find(tmp.string()) == 0);
    CHECK(p.find("autowhisper-settings-") != std::string::npos);
    CHECK(p.ends_with(".info"));

    fs::remove_all(tmp);
    if (saved) set_env_var("XDG_RUNTIME_DIR", saved);
    else unset_env_var("XDG_RUNTIME_DIR");
}

TEST_CASE("sidecar_path_for falls back to writable temp dir with user suffix when no XDG", "[sidecar]") {
    const char* saved = std::getenv("XDG_RUNTIME_DIR");
    unset_env_var("XDG_RUNTIME_DIR");

    auto p = sidecar_path_for("/x.toml");
    CHECK(p.find("autowhisper-settings-") != std::string::npos);
    CHECK(p.ends_with(".info"));
#if defined(_WIN32)
    CHECK(p.find(stable_temp_dir().string()) != std::string::npos);
#else
    std::error_code ec;
    CHECK(fs::weakly_canonical(fs::path(p).parent_path(), ec) == fs::weakly_canonical(stable_temp_dir(), ec));
    CHECK(p.find("-" + std::to_string(::getuid()) + "-") != std::string::npos);
#endif

    if (saved) set_env_var("XDG_RUNTIME_DIR", saved);
}

TEST_CASE("parse_sidecar round-trip with format_sidecar", "[sidecar]") {
    SidecarContents c{12345, 38543, "/home/u/.config/autowhisper/config.toml"};
    auto s = format_sidecar(c);
    auto parsed = parse_sidecar(s);
    REQUIRE(parsed.has_value());
    CHECK(parsed->pid == 12345);
    CHECK(parsed->port == 38543);
    CHECK(parsed->canonical_path == c.canonical_path);
}

TEST_CASE("parse_sidecar rejects empty, malformed, or out-of-range input", "[sidecar]") {
    CHECK_FALSE(parse_sidecar("").has_value());
    CHECK_FALSE(parse_sidecar("onlyoneline").has_value());
    CHECK_FALSE(parse_sidecar("notanumber\n38543\n/x\n").has_value());
    CHECK_FALSE(parse_sidecar("123\n99999\n/x\n").has_value());
    CHECK_FALSE(parse_sidecar("0\n38543\n/x\n").has_value());
    CHECK_FALSE(parse_sidecar("123\n0\n/x\n").has_value());
}

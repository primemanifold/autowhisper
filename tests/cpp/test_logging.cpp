#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "util/logging.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {
int test_process_id() {
#if defined(_WIN32)
    return _getpid();
#else
    return ::getpid();
#endif
}
}  // namespace


namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

struct TempLogPath {
    fs::path path;

    explicit TempLogPath(const std::string& suffix) {
        path = fs::temp_directory_path() /
               ("autowhisper_logtest_" +
                std::to_string(test_process_id()) + "_" +
                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) +
                "_" + suffix);
    }

    ~TempLogPath() {
        std::error_code ec;
        fs::remove(path, ec);
    }

    TempLogPath(const TempLogPath&) = delete;
    TempLogPath& operator=(const TempLogPath&) = delete;
};

} // namespace

TEST_CASE("setup_logging maps level strings to spdlog levels", "[logging]") {
    struct Case {
        const char* input;
        spdlog::level::level_enum expected;
    };

    auto c = GENERATE(
        Case{"trace",   spdlog::level::trace},
        Case{"debug",   spdlog::level::debug},
        Case{"info",    spdlog::level::info},
        Case{"warn",    spdlog::level::warn},
        Case{"warning", spdlog::level::warn},
        Case{"error",   spdlog::level::err}
    );

    setup_logging(c.input, "");
    CHECK(spdlog::default_logger()->level() == c.expected);
}

TEST_CASE("setup_logging falls back to info on unknown level", "[logging]") {
    // Unknown and case-mismatched strings all fall through to the default.
    auto level = GENERATE("", "verbose", "silly", "TRACE", "Debug", "fatal");

    setup_logging(level, "");
    CHECK(spdlog::default_logger()->level() == spdlog::level::info);
}

TEST_CASE("setup_logging names the default logger 'autowhisper'", "[logging]") {
    setup_logging("info", "");
    CHECK(spdlog::default_logger()->name() == "autowhisper");
}

TEST_CASE("setup_logging replaces the default logger on each call", "[logging]") {
    setup_logging("info", "");
    auto first = spdlog::default_logger();

    setup_logging("debug", "");
    auto second = spdlog::default_logger();

    // New logger instance each time, and the level follows the latest call.
    CHECK(first.get() != second.get());
    CHECK(second->level() == spdlog::level::debug);
}

TEST_CASE("setup_logging writes to log file when path is provided", "[logging]") {
    TempLogPath tmp("writes.log");

    setup_logging("info", tmp.path.string());
    spdlog::info("probe line autowhisper-logtest");
    spdlog::default_logger()->flush();

    REQUIRE(fs::exists(tmp.path));
    std::ifstream ifs(tmp.path);
    std::string contents((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
    CHECK(contents.find("probe line autowhisper-logtest") != std::string::npos);
}

TEST_CASE("setup_logging does not throw on unwritable log path", "[logging]") {
    // File sink construction will fail (no such directory); the implementation
    // catches spdlog_ex and continues with the console sink only.
    REQUIRE_NOTHROW(
        setup_logging("info", "/nonexistent_autowhisper_testdir/should_fail.log")
    );
}

TEST_CASE("setup_logging with empty log_file still configures level", "[logging]") {
    setup_logging("debug", "");
    CHECK(spdlog::default_logger()->level() == spdlog::level::debug);
    CHECK(spdlog::default_logger()->name() == "autowhisper");
}

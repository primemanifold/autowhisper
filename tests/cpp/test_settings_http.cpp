#include <catch2/catch_test_macros.hpp>

#include "config/config.h"
#include "config/schema.h"
#include "platform/capabilities.h"
#include "settings/handlers.h"
#include "settings/server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>

namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

constexpr const char* kTestToken =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

struct ServerFixture {
    httplib::Server srv;
    int port = 0;
    std::thread thread;
    ServerFixture(const std::string& path) {
        settings::attach_api_routes(srv, path, kTestToken);
        port = srv.bind_to_any_port("127.0.0.1");
        REQUIRE(port > 0);
        thread = std::thread([this]() { srv.listen_after_bind(); });
        for (int i = 0; i < 50 && !srv.is_running(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE(srv.is_running());
    }
    ~ServerFixture() {
        srv.stop();
        if (thread.joinable()) thread.join();
    }

    // Client pre-armed with the session token, as the web app would be.
    httplib::Client client() const {
        httplib::Client cli("127.0.0.1", port);
        cli.set_default_headers({{"Authorization", std::string("Bearer ") + kTestToken}});
        return cli;
    }
};

struct TempDir {
    fs::path path;
    TempDir() {
        fs::path base;
        std::error_code ec;
        base = fs::temp_directory_path(ec);
        if (ec || !fs::is_directory(base)) {
            base = fs::path("/tmp");
        }
        path = base / ("aw_http_" + std::to_string(::getpid()) + "_" +
            std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

}  // namespace

TEST_CASE("GET /api/schema returns schema JSON", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    std::ofstream(path) << "[model]\n";
    ServerFixture s(path);

    auto cli = s.client();
    auto res = cli.Get("/api/schema");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j.contains("model"));
    CHECK(j["model"].is_array());
}

TEST_CASE("GET /api/defaults matches Config::default_config", "[settings_http]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());
    std::ofstream((tmp.path / "c.toml")) << "";

    auto cli = s.client();
    auto res = cli.Get("/api/defaults");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j["model"]["size"] == Config::default_config().model.size);
}

TEST_CASE("GET /api/platform returns desktop capability diagnostics", "[settings_http]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());

    auto cli = s.client();
    auto res = cli.Get("/api/platform");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j["platform"].is_string());
    CHECK(j["build_target"].is_string());
    REQUIRE(j["features"].is_array());
    CHECK(j["features"].size() >= 4);
    CHECK(j["summary"].is_string());
}

TEST_CASE("GET /api/config returns defaults for nonexistent file", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "sub" / "new.toml").string();
    ServerFixture s(path);

    auto cli = s.client();
    auto res = cli.Get("/api/config");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j["model"]["size"] == Config::default_config().model.size);

    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("PUT /api/config with valid body writes TOML and creates parents", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "sub1" / "sub2" / "new.toml").string();
    ServerFixture s(path);

    auto body = settings::defaults_json();
    body["model"]["size"] = "tiny.en";

    auto cli = s.client();
    auto res = cli.Put("/api/config", body.dump(), "application/json");
    REQUIRE(res);
    CHECK(res->status == 204);

    REQUIRE(fs::exists(path));
    auto loaded = Config::load(path);
    CHECK(loaded.model.size == "tiny.en");

    auto got = cli.Get("/api/config");
    REQUIRE(got);
    auto jget = nlohmann::json::parse(got->body);
    CHECK(jget["model"]["size"] == "tiny.en");
}

TEST_CASE("PUT /api/config with invalid enum returns 400 and does not write", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    ServerFixture s(path);

    auto body = settings::defaults_json();
    body["model"]["size"] = "not-a-real-model";

    auto cli = s.client();
    auto res = cli.Put("/api/config", body.dump(), "application/json");
    REQUIRE(res);
    CHECK(res->status == 400);

    auto j = nlohmann::json::parse(res->body);
    CHECK(j.contains("errors"));
    REQUIRE(j["errors"].is_array());
    REQUIRE(!j["errors"].empty());
    CHECK(std::string(j["errors"][0]).find("Invalid model size") != std::string::npos);

    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("PUT /api/config 400 response includes structured issues array", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    ServerFixture s(path);

    auto body = settings::defaults_json();
    body["model"]["size"] = "not-a-real-model";

    auto cli = s.client();
    auto res = cli.Put("/api/config", body.dump(), "application/json");
    REQUIRE(res);
    CHECK(res->status == 400);

    auto j = nlohmann::json::parse(res->body);
    REQUIRE(j.contains("issues"));
    REQUIRE(j["issues"].is_array());
    REQUIRE(!j["issues"].empty());
    auto& iss = j["issues"][0];
    CHECK(iss["severity"] == "error");
    CHECK(iss["path"] == "model.size");
    CHECK(iss["code"] == "invalid_enum");
    CHECK(std::string(iss["message"]).find("Invalid model size") != std::string::npos);
}

TEST_CASE("PUT /api/config malformed JSON returns 400 with json_parse_error issue", "[settings_http]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    ServerFixture s(path);

    auto cli = s.client();
    auto res = cli.Put("/api/config", "{not-json", "application/json");
    REQUIRE(res);
    CHECK(res->status == 400);

    auto j = nlohmann::json::parse(res->body);
    REQUIRE(j.contains("issues"));
    REQUIRE(j["issues"].is_array());
    REQUIRE(!j["issues"].empty());
    CHECK(j["issues"][0]["severity"] == "error");
    CHECK(j["issues"][0]["code"] == "json_parse_error");
}

TEST_CASE("API requests without a token are rejected with 401", "[settings_http][auth]") {
    TempDir tmp;
    auto path = (tmp.path / "c.toml").string();
    ServerFixture s(path);

    httplib::Client bare("127.0.0.1", s.port);

    auto get = bare.Get("/api/config");
    REQUIRE(get);
    CHECK(get->status == 401);

    auto body = settings::defaults_json();
    auto put = bare.Put("/api/config", body.dump(), "application/json");
    REQUIRE(put);
    CHECK(put->status == 401);
    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("API requests with a wrong token are rejected with 401", "[settings_http][auth]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());

    httplib::Client bad("127.0.0.1", s.port);
    bad.set_default_headers({{"Authorization", "Bearer wrong-token"}});
    auto res = bad.Get("/api/schema");
    REQUIRE(res);
    CHECK(res->status == 401);
}

TEST_CASE("Token is accepted via query parameter", "[settings_http][auth]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());

    httplib::Client bare("127.0.0.1", s.port);
    auto res = bare.Get((std::string("/api/schema?token=") + kTestToken).c_str());
    REQUIRE(res);
    CHECK(res->status == 200);
}

TEST_CASE("Non-loopback Host header is rejected with 403 (DNS rebinding)", "[settings_http][auth]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());

    httplib::Client cli("127.0.0.1", s.port);
    httplib::Headers headers{
        {"Host", "evil.example.com"},
        {"Authorization", std::string("Bearer ") + kTestToken},
    };
    auto res = cli.Get("/api/schema", headers);
    REQUIRE(res);
    CHECK(res->status == 403);
}

TEST_CASE("attach_api_routes refuses an empty token", "[settings_http][auth]") {
    httplib::Server srv;
    CHECK_THROWS_AS(settings::attach_api_routes(srv, "/tmp/c.toml", ""),
                    std::invalid_argument);
}

TEST_CASE("generate_session_token yields unique 64-char hex tokens", "[settings_http][auth]") {
    std::set<std::string> seen;
    for (int i = 0; i < 16; i++) {
        auto token = settings::generate_session_token();
        REQUIRE(token.size() == 64);
        for (char c : token) {
            bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            REQUIRE(hex);
        }
        seen.insert(token);
    }
    CHECK(seen.size() == 16);
}

TEST_CASE("is_loopback_host accepts loopback origins only", "[settings_http][auth]") {
    using settings::is_loopback_host;
    CHECK(is_loopback_host("127.0.0.1"));
    CHECK(is_loopback_host("127.0.0.1:8080"));
    CHECK(is_loopback_host("localhost"));
    CHECK(is_loopback_host("localhost:9000"));
    CHECK(is_loopback_host("[::1]:9000"));
    CHECK_FALSE(is_loopback_host(""));
    CHECK_FALSE(is_loopback_host("evil.example.com"));
    CHECK_FALSE(is_loopback_host("localhost.evil.com"));
    CHECK_FALSE(is_loopback_host("127.0.0.1.evil.com"));
    CHECK_FALSE(is_loopback_host("[::2]:9000"));
}

TEST_CASE("token_matches enforces exact bearer or query token", "[settings_http][auth]") {
    using settings::token_matches;
    const std::string tok = "aabbcc";
    CHECK(token_matches(tok, "Bearer aabbcc", ""));
    CHECK(token_matches(tok, "", "aabbcc"));
    CHECK_FALSE(token_matches(tok, "Bearer aabbc", ""));
    CHECK_FALSE(token_matches(tok, "Bearer aabbccd", ""));
    CHECK_FALSE(token_matches(tok, "aabbcc", ""));
    CHECK_FALSE(token_matches(tok, "", ""));
    CHECK_FALSE(token_matches("", "", ""));
}

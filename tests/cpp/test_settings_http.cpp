#include <catch2/catch_test_macros.hpp>

#include "config/config.h"
#include "config/schema.h"
#include "platform/capabilities.h"
#include "settings/handlers.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

void register_routes(httplib::Server& srv, const std::string& path) {
    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& r) {
        r.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& r) {
        r.set_content(settings::defaults_json().dump(), "application/json");
    });
    srv.Get("/api/platform", [](const httplib::Request&, httplib::Response& r) {
        r.set_content(platform_capabilities_json(current_platform_capabilities()).dump(), "application/json");
    });
    srv.Get("/api/config", [path](const httplib::Request&, httplib::Response& r) {
        try {
            r.set_content(settings::get_config_json(path).dump(), "application/json");
        } catch (const std::exception& e) {
            r.status = 500;
            r.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    srv.Put("/api/config", [path](const httplib::Request& req, httplib::Response& r) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (const std::exception& e) {
            r.status = 400;
            r.set_content(nlohmann::json{
                {"errors", {e.what()}},
                {"issues", nlohmann::json::array({{
                    {"severity", "error"}, {"path", ""}, {"code", "json_parse_error"},
                    {"message", e.what()}}})}}.dump(), "application/json");
            return;
        }
        auto v = settings::validate_json(body);
        if (!v.ok()) {
            r.status = 400;
            nlohmann::json issues = nlohmann::json::array();
            for (const auto& iss : v.issues) issues.push_back(settings::issue_to_json(iss));
            r.set_content(nlohmann::json{
                {"errors", v.errors}, {"issues", std::move(issues)}}.dump(),
                "application/json");
            return;
        }
        settings::save_config_json(path, body);
        r.status = 204;
    });
}

struct ServerFixture {
    httplib::Server srv;
    int port = 0;
    std::thread thread;
    ServerFixture(const std::string& path) {
        register_routes(srv, path);
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

    httplib::Client cli("127.0.0.1", s.port);
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

    httplib::Client cli("127.0.0.1", s.port);
    auto res = cli.Get("/api/defaults");
    REQUIRE(res);
    CHECK(res->status == 200);
    auto j = nlohmann::json::parse(res->body);
    CHECK(j["model"]["size"] == Config::default_config().model.size);
}

TEST_CASE("GET /api/platform returns desktop capability diagnostics", "[settings_http]") {
    TempDir tmp;
    ServerFixture s((tmp.path / "c.toml").string());

    httplib::Client cli("127.0.0.1", s.port);
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

    httplib::Client cli("127.0.0.1", s.port);
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

    httplib::Client cli("127.0.0.1", s.port);
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

    httplib::Client cli("127.0.0.1", s.port);
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

    httplib::Client cli("127.0.0.1", s.port);
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

    httplib::Client cli("127.0.0.1", s.port);
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

#include "settings/server.h"

#include "config/config.h"
#include "config/schema.h"
#include "platform/capabilities.h"
#include "settings/handlers.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <random>
#include <stdexcept>

namespace autowhisper::settings {

namespace {

// Constant-time comparison so the token cannot be guessed byte-by-byte
// through response timing.
bool equals_constant_time(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

void set_unauthorized(httplib::Response& res) {
    res.status = 401;
    res.set_content(
        nlohmann::json{{"error",
                        "Missing or invalid session token. Open the settings UI "
                        "through the URL printed by 'autowhisper config ui'."}}
            .dump(),
        "application/json");
}

}  // namespace

std::string generate_session_token() {
    // std::random_device is OS-entropy backed on glibc, macOS, and MSVC.
    // Mix in time and address-space noise as a hedge against weak
    // implementations.
    std::random_device rd;
    std::seed_seq seq{
        rd(), rd(), rd(), rd(),
        static_cast<unsigned>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
        static_cast<unsigned>(
            std::chrono::system_clock::now().time_since_epoch().count()),
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(&seq)),
    };
    std::mt19937_64 gen(seq);
    static const char* hex = "0123456789abcdef";
    std::string token;
    token.reserve(64);
    for (int word = 0; word < 4; word++) {
        uint64_t v = gen();
        for (int shift = 60; shift >= 0; shift -= 4) {
            token.push_back(hex[(v >> shift) & 0xf]);
        }
    }
    return token;
}

bool is_loopback_host(const std::string& host_header) {
    if (host_header.empty()) return false;

    std::string host = host_header;
    if (!host.empty() && host.front() == '[') {
        // Bracketed IPv6 literal, e.g. "[::1]:8080".
        auto close = host.find(']');
        if (close == std::string::npos) return false;
        host = host.substr(1, close - 1);
        return host == "::1";
    }
    auto colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    return host == "127.0.0.1" || host == "localhost";
}

bool token_matches(const std::string& expected,
                   const std::string& authorization_header,
                   const std::string& token_query_param) {
    if (expected.empty()) return false;
    const std::string bearer_prefix = "Bearer ";
    if (authorization_header.rfind(bearer_prefix, 0) == 0 &&
        equals_constant_time(authorization_header.substr(bearer_prefix.size()), expected)) {
        return true;
    }
    return equals_constant_time(token_query_param, expected);
}

void attach_api_routes(httplib::Server& srv,
                       const std::string& config_path,
                       const std::string& token) {
    if (token.empty()) {
        throw std::invalid_argument("settings server requires a non-empty session token");
    }

    srv.set_pre_routing_handler([token](const httplib::Request& req, httplib::Response& res) {
        if (!is_loopback_host(req.get_header_value("Host"))) {
            res.status = 403;
            res.set_content(nlohmann::json{{"error", "Forbidden host"}}.dump(),
                            "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }
        if (req.path.rfind("/api/", 0) == 0) {
            const std::string auth = req.get_header_value("Authorization");
            const std::string query_token =
                req.has_param("token") ? req.get_param_value("token") : "";
            if (!token_matches(token, auth, query_token)) {
                set_unauthorized(res);
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(defaults_json().dump(), "application/json");
    });
    srv.Get("/api/platform", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(platform_capabilities_json(current_platform_capabilities()).dump(),
                        "application/json");
    });
    srv.Get("/api/models", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(models_json().dump(), "application/json");
    });
    srv.Get("/api/config", [config_path](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(get_config_json(config_path).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });
    srv.Put("/api/config", [config_path](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.body);
        } catch (const std::exception& e) {
            res.status = 400;
            const auto message = std::string(e.what());
            res.set_content(nlohmann::json{
                                {"errors", {message}},
                                {"issues", {issue_to_json(ValidationIssue{
                                                ValidationSeverity::Error,
                                                "",
                                                "json_parse_error",
                                                message})}}}
                                .dump(),
                            "application/json");
            return;
        }
        auto v = validate_json(body);
        if (!v.ok()) {
            nlohmann::json issues = nlohmann::json::array();
            for (const auto& issue : v.issues) {
                issues.push_back(issue_to_json(issue));
            }
            res.status = 400;
            res.set_content(nlohmann::json{{"errors", v.errors}, {"issues", issues}}.dump(),
                            "application/json");
            return;
        }
        try {
            save_config_json(config_path, body);
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
            return;
        }
        res.status = 204;
    });
}

}  // namespace autowhisper::settings

#include "cli/cli.h"
#include "config/config.h"
#include "config/schema.h"
#include "settings/assets.h"
#include "settings/handlers.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

namespace autowhisper {

namespace {

void register_api_routes(httplib::Server& srv, const std::string& config_path) {
    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(settings::defaults_json().dump(), "application/json");
    });
    srv.Get("/api/config", [config_path](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(settings::get_config_json(config_path).dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(),
                            "application/json");
        }
    });
    srv.Put("/api/config", [config_path](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.body);
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"errors", {e.what()}}}.dump(),
                            "application/json");
            return;
        }
        auto v = settings::validate_json(body);
        if (!v.ok()) {
            res.status = 400;
            res.set_content(nlohmann::json{{"errors", v.errors}}.dump(),
                            "application/json");
            return;
        }
        try {
            settings::save_config_json(config_path, body);
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(),
                            "application/json");
            return;
        }
        res.status = 204;
    });
    srv.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kIndexHtml), "text/html");
    });
    srv.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kStyleCss), "text/css");
    });
    srv.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kAppJs), "application/javascript");
    });
}

}  // namespace

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string config_path = config_path_opt.empty()
                                      ? get_user_config_path()
                                      : config_path_opt;

    httplib::Server srv;
    register_api_routes(srv, config_path);

    int port = srv.bind_to_any_port("127.0.0.1");
    if (port < 0) {
        std::cerr << "Failed to bind settings UI to localhost\n";
        return 1;
    }

    std::cout << "AutoWhisper Settings: http://127.0.0.1:" << port << "\n";
    std::cout << "Config file: " << config_path << "\n";
    std::cout << "Press Ctrl+C to close\n";

    srv.listen_after_bind();
    return 0;
}

}  // namespace autowhisper

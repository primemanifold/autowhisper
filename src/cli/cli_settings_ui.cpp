#include "cli/cli.h"
#include "config/config.h"
#include "config/schema.h"
#include "settings/handlers.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

namespace autowhisper {

namespace {

void register_api_routes(httplib::Server& srv) {
    srv.Get("/api/schema", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(schema::to_json().dump(), "application/json");
    });
    srv.Get("/api/defaults", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(settings::defaults_json().dump(), "application/json");
    });
}

}  // namespace

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string config_path = config_path_opt.empty()
                                      ? get_user_config_path()
                                      : config_path_opt;

    httplib::Server srv;
    register_api_routes(srv);

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

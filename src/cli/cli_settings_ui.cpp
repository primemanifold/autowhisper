#include "cli/cli.h"
#include "config/config.h"
#include "config/schema.h"
#include "settings/assets.h"
#include "settings/handlers.h"
#include "settings/sidecar.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

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
}

std::string read_all(int fd) {
    std::string out;
    char buf[1024];
    if (lseek(fd, 0, SEEK_SET) < 0) return out;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, buf + n);
    }
    return out;
}

void write_full(int fd, const std::string& data) {
    if (ftruncate(fd, 0) < 0) return;
    if (lseek(fd, 0, SEEK_SET) < 0) return;
    size_t written = 0;
    while (written < data.size()) {
        ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        if (n <= 0) return;
        written += n;
    }
    fsync(fd);
}

int open_sidecar_locked(const std::string& path, bool* got_lock) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return -1;
    if (::flock(fd, LOCK_EX | LOCK_NB) == 0) { *got_lock = true; return fd; }
    *got_lock = false;
    return fd;
}

}  // namespace

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    (void)open_browser;
    const std::string config_path = config_path_opt.empty()
                                      ? get_user_config_path()
                                      : config_path_opt;
    const std::string canonical = settings::weak_canonical(config_path);
    const std::string sidecar = settings::sidecar_path_for(canonical);

    bool got_lock = false;
    int fd = open_sidecar_locked(sidecar, &got_lock);
    if (fd < 0) {
        std::cerr << "Failed to open sidecar: " << sidecar << "\n";
        return 1;
    }

    if (!got_lock) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
                got_lock = true;
                break;
            }
            auto parsed = settings::parse_sidecar(read_all(fd));
            if (parsed.has_value()) {
                if (parsed->canonical_path == canonical) {
                    std::cout << "Settings UI already running at http://127.0.0.1:"
                              << parsed->port << "\n";
                    ::close(fd);
                    return 0;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!got_lock) {
            std::cerr << "Settings UI unresponsive (sidecar held but not published)\n";
            ::close(fd);
            return 1;
        }
    }

    if (::ftruncate(fd, 0) < 0) {
        std::cerr << "Failed to truncate sidecar: " << sidecar << "\n";
        ::close(fd);
        return 1;
    }

    httplib::Server srv;
    register_api_routes(srv, canonical);
    srv.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kIndexHtml), "text/html");
    });
    srv.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kStyleCss), "text/css");
    });
    srv.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(settings::assets::kAppJs), "application/javascript");
    });

    int port = srv.bind_to_any_port("127.0.0.1");
    if (port < 0) {
        std::cerr << "Failed to bind settings UI to localhost\n";
        ::close(fd);
        return 1;
    }

    settings::SidecarContents contents{::getpid(), port, canonical};
    write_full(fd, settings::format_sidecar(contents));

    std::cout << "AutoWhisper Settings: http://127.0.0.1:" << port << "\n";
    std::cout << "Config file: " << canonical << "\n";
    std::cout << "Press Ctrl+C to close\n";

    srv.listen_after_bind();
    ::close(fd);
    return 0;
}

}  // namespace autowhisper

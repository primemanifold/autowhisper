#include "cli/cli.h"
#include "config/config.h"
#include "config/schema.h"
#include "settings/assets.h"
#include "platform/capabilities.h"
#include "settings/handlers.h"
#include "settings/server.h"
#include "settings/sidecar.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace autowhisper {

#if defined(_WIN32)

int cmd_config_ui(const std::string&, bool) {
    std::cerr << "Settings UI server is not implemented on Windows yet. "
              << "Use the desktop platform diagnostics to track Windows readiness.\n";
    return 1;
}

#else

namespace {

std::string settings_url(int port, const std::string& token) {
    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    if (!token.empty()) url += "?token=" + token;
    return url;
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

static int g_signal_pipe[2] = {-1, -1};
static httplib::Server* g_server = nullptr;

void shutdown_signal_handler(int /*signo*/) {
    if (g_signal_pipe[1] >= 0) {
        char c = 'x';
        (void)!::write(g_signal_pipe[1], &c, 1);
    }
}

void launch_browser(const std::string& url) {
    pid_t pid = ::fork();
    if (pid < 0) {
        spdlog::warn("fork failed; cannot launch browser");
        return;
    }
    if (pid == 0) {
        if (::fork() == 0) {
            ::setsid();
            int devnull = ::open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                ::dup2(devnull, 0); ::dup2(devnull, 1); ::dup2(devnull, 2);
                if (devnull > 2) ::close(devnull);
            }
#if defined(__APPLE__)
            // macOS: `open` delegates to LaunchServices (default browser).
            ::execlp("open", "open", url.c_str(), (char*)nullptr);
#else
            ::execlp("xdg-open", "xdg-open", url.c_str(), (char*)nullptr);
#endif
            ::_exit(127);
        }
        ::_exit(0);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
}

bool install_signal_pipe_and_handlers() {
    if (::pipe(g_signal_pipe) < 0) return false;
    ::fcntl(g_signal_pipe[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(g_signal_pipe[1], F_SETFD, FD_CLOEXEC);
    int flags = ::fcntl(g_signal_pipe[1], F_GETFL, 0);
    ::fcntl(g_signal_pipe[1], F_SETFL, flags | O_NONBLOCK);

    struct sigaction sa{};
    sa.sa_handler = shutdown_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    return true;
}

}  // namespace

int cmd_config_ui(const std::string& config_path_opt, bool open_browser) {
    const std::string config_path = config_path_opt.empty()
                                      ? resolve_config_path()
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
                    const std::string url = settings_url(parsed->port, parsed->token);
                    std::cout << "Settings UI already running at " << url << "\n";
                    if (open_browser) {
                        launch_browser(url);
                    }
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

    const std::string token = settings::generate_session_token();

    httplib::Server srv;
    settings::attach_api_routes(srv, canonical, token);
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

    g_server = &srv;
    if (!install_signal_pipe_and_handlers()) {
        std::cerr << "Failed to install signal-pipe shutdown\n";
        ::close(fd);
        return 1;
    }

    std::thread watcher([]() {
        char buf[1];
        while (::read(g_signal_pipe[0], buf, 1) <= 0) {
            if (errno != EINTR) break;
        }
        if (g_server) g_server->stop();
    });

    settings::SidecarContents contents{::getpid(), port, canonical, token};
    write_full(fd, settings::format_sidecar(contents));

    ::setsid();

    std::cout << "AutoWhisper Settings: " << settings_url(port, token) << "\n";
    std::cout << "Config file: " << canonical << "\n";
    std::cout << "Press Ctrl+C to close\n";

    if (open_browser) {
        launch_browser(settings_url(port, token));
    }

    srv.listen_after_bind();
    watcher.join();
    if (g_signal_pipe[0] >= 0) ::close(g_signal_pipe[0]);
    if (g_signal_pipe[1] >= 0) ::close(g_signal_pipe[1]);
    ::close(fd);
    return 0;
}

#endif

}  // namespace autowhisper

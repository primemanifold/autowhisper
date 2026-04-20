#include "service/service.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

namespace autowhisper {

namespace {

constexpr const char* kLabel = "us.primemanifold.autowhisper";

std::string home_dir() {
    if (const char* h = std::getenv("HOME")) return h;
    return "";
}

std::string plist_path() {
    return home_dir() + "/Library/LaunchAgents/" + std::string(kLabel) + ".plist";
}

std::string log_path() {
    return home_dir() + "/Library/Logs/autowhisper/autowhisper.log";
}

std::string uid_str() {
    return std::to_string(getuid());
}

std::string target() {
    // launchctl target for gui domain of current user, service label.
    return "gui/" + uid_str() + "/" + kLabel;
}

std::string domain() {
    return "gui/" + uid_str();
}

ServiceResult to_service_result(const ProcessResult& pr) {
    return ServiceResult{pr.exit_code, pr.stdout_str, pr.stderr_str};
}

// Returns the stdout of `launchctl print <target>`, or empty string if the
// label is not loaded.
std::string launchctl_print() {
    auto r = run_command({"launchctl", "print", target()}, 5);
    if (r.exit_code != 0) return "";
    return r.stdout_str;
}

// Is a service with our label currently loaded?
bool is_loaded() {
    return !launchctl_print().empty();
}

// Does the loaded plist point at plist_path()?
bool loaded_matches(const std::string& expected_plist) {
    auto info = launchctl_print();
    if (info.empty()) return false;
    // `launchctl print` reports a `path = <plist path>` line.
    return info.find(expected_plist) != std::string::npos;
}

class LaunchdServiceManager : public ServiceManager {
public:
    ServiceResult start() override {
        const std::string plist = plist_path();
        if (!fs::exists(plist)) {
            return ServiceResult{
                1, "",
                "Service plist not installed at " + plist +
                "\nRun the install script or `autowhisper install` first."
            };
        }

        if (is_loaded()) {
            if (!loaded_matches(plist)) {
                spdlog::info("Stale launchd job detected; reloading");
                run_command({"launchctl", "bootout", target()}, 10);
            } else {
                // Already loaded and matches — just kickstart.
                return to_service_result(
                    run_command({"launchctl", "kickstart", "-k", target()}, 10));
            }
        }

        auto pr = run_command({"launchctl", "bootstrap", domain(), plist}, 10);
        // Exit 17 == EEXIST (already loaded). Treat as success.
        if (pr.exit_code == 17) {
            pr.exit_code = 0;
        }
        return to_service_result(pr);
    }

    ServiceResult stop() override {
        auto pr = run_command({"launchctl", "bootout", target()}, 10);
        // Exit 5 == ESRCH (not loaded). Treat as success.
        if (pr.exit_code == 5) {
            pr.exit_code = 0;
        }
        return to_service_result(pr);
    }

    ServiceResult restart() override {
        // Bootstrap if not loaded, otherwise kickstart.
        if (is_loaded()) {
            return to_service_result(
                run_command({"launchctl", "kickstart", "-k", target()}, 10));
        }
        return start();
    }

    ServiceResult status() override {
        auto pr = run_command({"launchctl", "print", target()}, 5);
        if (pr.exit_code != 0) {
            return ServiceResult{1, "", "Service not loaded (" + target() + ")"};
        }
        return to_service_result(pr);
    }

    int tail_logs(bool follow, int lines) override {
        const std::string log = log_path();
        if (!fs::exists(log)) {
            spdlog::warn("Log file not found: {}", log);
            return 1;
        }
        std::vector<std::string> cmd = {"tail"};
        cmd.push_back("-n");
        cmd.push_back(std::to_string(lines));
        if (follow) cmd.push_back("-f");
        cmd.push_back(log);
        return run_passthrough(cmd);
    }

    std::string backend_name() const override { return "launchd"; }
};

} // namespace

std::unique_ptr<ServiceManager> make_service_manager() {
    return std::make_unique<LaunchdServiceManager>();
}

} // namespace autowhisper

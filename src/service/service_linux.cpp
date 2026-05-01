#include "service/service.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace autowhisper {

namespace {

constexpr const char* kServiceName = "autowhisper";

ProcessResult run_systemctl(const std::vector<std::string>& args) {
    std::vector<std::string> cmd = {"systemctl", "--user"};
    cmd.insert(cmd.end(), args.begin(), args.end());
    return run_command(cmd, 10);
}

ServiceResult to_service_result(const ProcessResult& pr) {
    return ServiceResult{pr.exit_code, pr.stdout_str, pr.stderr_str};
}

class SystemdServiceManager : public ServiceManager {
public:
    ServiceResult start() override {
        return to_service_result(run_systemctl({"start", kServiceName}));
    }
    ServiceResult stop() override {
        return to_service_result(run_systemctl({"stop", kServiceName}));
    }
    ServiceResult restart() override {
        return to_service_result(run_systemctl({"restart", kServiceName}));
    }
    ServiceResult status() override {
        return to_service_result(run_systemctl({"status", kServiceName}));
    }
    int tail_logs(bool follow, int lines) override {
        std::vector<std::string> cmd = {
            "journalctl", "--user", "-u", kServiceName,
            "-n", std::to_string(lines)
        };
        if (follow) cmd.push_back("-f");
        return run_passthrough(cmd);
    }
    std::string backend_name() const override { return "systemd"; }
};

} // namespace

std::unique_ptr<ServiceManager> make_service_manager() {
    return std::make_unique<SystemdServiceManager>();
}

} // namespace autowhisper

#pragma once

#include <memory>
#include <string>

namespace autowhisper {

struct ServiceResult {
    int exit_code = 0;
    std::string stdout_str;
    std::string stderr_str;
};

class ServiceManager {
public:
    virtual ~ServiceManager() = default;

    // Load and start the service (idempotent).
    virtual ServiceResult start() = 0;

    // Stop and unload the service (idempotent).
    virtual ServiceResult stop() = 0;

    // Restart (reload config from disk and kick the process).
    virtual ServiceResult restart() = 0;

    // Human-readable status. exit_code==0 means loaded and healthy.
    virtual ServiceResult status() = 0;

    // Tail logs. Runs in-process via passthrough; returns the child exit code.
    virtual int tail_logs(bool follow, int lines) = 0;

    // Returns a short name like "systemd" or "launchd".
    virtual std::string backend_name() const = 0;
};

std::unique_ptr<ServiceManager> make_service_manager();

} // namespace autowhisper

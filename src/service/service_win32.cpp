#include "service/service.h"

#include <memory>
#include <string>

namespace autowhisper {

namespace {

ServiceResult unsupported(const char* action) {
    return ServiceResult{
        1,
        "",
        std::string("Windows service ") + action +
            " is not implemented yet. Track Windows runtime readiness in desktop platform diagnostics."
    };
}

class Win32ServiceManager : public ServiceManager {
public:
    ServiceResult start() override { return unsupported("start"); }
    ServiceResult stop() override { return unsupported("stop"); }
    ServiceResult restart() override { return unsupported("restart"); }
    ServiceResult status() override { return unsupported("status"); }
    int tail_logs(bool, int) override { return 1; }
    std::string backend_name() const override { return "win32-placeholder"; }
};

} // namespace

std::unique_ptr<ServiceManager> make_service_manager() {
    return std::make_unique<Win32ServiceManager>();
}

} // namespace autowhisper

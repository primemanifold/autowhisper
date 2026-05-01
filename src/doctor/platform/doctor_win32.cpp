#include "doctor/doctor.h"

#include <vector>

namespace autowhisper {

std::vector<CheckGroup> platform_checks(bool) {
    return {
        CheckGroup{
            "Windows",
            {
                CheckResult{
                    CheckStatus::FAIL,
                    "Windows runtime checks are not implemented yet",
                    "This build proves Windows artifact generation only; runtime validation still requires a Windows host, VM, or proven Wine-capable x86_64 runner.",
                    "Run AutoWhisper on a real Windows validation host before claiming Windows runtime readiness."
                }
            }
        }
    };
}

} // namespace autowhisper

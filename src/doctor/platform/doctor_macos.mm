#include "doctor/doctor.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AVFoundation/AVFoundation.h>
#import <Metal/Metal.h>

#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <mach-o/dyld.h>

namespace autowhisper {

namespace {

CheckGroup check_metal_gpu() {
    CheckGroup g;
    g.header = "Metal GPU";

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        g.results.push_back({
            CheckStatus::FAIL, "No Metal device detected", "",
            "Metal requires macOS 10.14+ with a supported GPU"
        });
        return g;
    }

    NSString* name = [device name];
    g.results.push_back({
        CheckStatus::INFO, "GPU:", name.UTF8String, ""
    });
    g.results.push_back({
        CheckStatus::OK, "Metal inference available", "", ""
    });
    return g;
}

CheckGroup check_permissions() {
    CheckGroup g;
    g.header = "Permissions (TCC)";

    // Input Monitoring — required for CGEventTap.
    bool listen_ok = CGPreflightListenEventAccess();
    g.results.push_back({
        listen_ok ? CheckStatus::OK : CheckStatus::FAIL,
        "Input Monitoring (global hotkey)", "",
        listen_ok ? "" :
            "System Settings \xe2\x86\x92 Privacy & Security \xe2\x86\x92 "
            "Input Monitoring \xe2\x86\x92 add AutoWhisper"
    });

    // Post-event access — required for CGEventPost (text injection / cmd+V).
    bool post_ok = CGPreflightPostEventAccess();
    g.results.push_back({
        post_ok ? CheckStatus::OK : CheckStatus::FAIL,
        "Accessibility (text injection)", "",
        post_ok ? "" :
            "System Settings \xe2\x86\x92 Privacy & Security \xe2\x86\x92 "
            "Accessibility \xe2\x86\x92 add AutoWhisper"
    });

    // Microphone — required for audio capture.
    auto mic = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    bool mic_ok = (mic == AVAuthorizationStatusAuthorized);
    std::string mic_msg;
    switch (mic) {
        case AVAuthorizationStatusNotDetermined: mic_msg = "not yet prompted"; break;
        case AVAuthorizationStatusRestricted:    mic_msg = "restricted";       break;
        case AVAuthorizationStatusDenied:        mic_msg = "denied";           break;
        case AVAuthorizationStatusAuthorized:    mic_msg = "granted";          break;
    }
    g.results.push_back({
        mic_ok ? CheckStatus::OK : CheckStatus::FAIL,
        "Microphone (audio capture) — " + mic_msg, "",
        mic_ok ? "" :
            "System Settings \xe2\x86\x92 Privacy & Security \xe2\x86\x92 "
            "Microphone \xe2\x86\x92 add AutoWhisper"
    });

    if (!listen_ok || !post_ok || !mic_ok) {
        g.results.push_back({
            CheckStatus::INFO,
            "After granting, relaunch AutoWhisper so the new permissions "
            "take effect.", "", ""
        });
    }
    return g;
}

CheckGroup check_audio() {
    CheckGroup g;
    g.header = "Audio";
    // CoreAudio devices are enumerated by miniaudio inside AudioManager;
    // here we just tell the user the mic permission gate matters.
    auto mic = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (mic != AVAuthorizationStatusAuthorized) {
        g.results.push_back({
            CheckStatus::WARN,
            "Microphone not yet authorized — device list will be empty",
            "", "Grant Microphone access (see Permissions above)"
        });
    } else {
        g.results.push_back({
            CheckStatus::OK, "CoreAudio capture available (via miniaudio)", "", ""
        });
    }
    return g;
}

CheckGroup check_service() {
    CheckGroup g;
    g.header = "Service";

    std::string label = "us.primemanifold.autowhisper";
    std::string target = "gui/" + std::to_string(getuid()) + "/" + label;

    auto print = run_command({"launchctl", "print", target}, 5);
    if (print.exit_code == 0) {
        g.results.push_back({ CheckStatus::OK, "LaunchAgent loaded", "", "" });
        // Best-effort: show whether it's currently running.
        if (print.stdout_str.find("state = running") != std::string::npos) {
            g.results.push_back({ CheckStatus::OK, "Process is running", "", "" });
        }
    } else {
        const char* home = std::getenv("HOME");
        std::string plist = std::string(home ? home : "") +
                            "/Library/LaunchAgents/" + label + ".plist";
        struct stat st;
        if (stat(plist.c_str(), &st) == 0) {
            g.results.push_back({
                CheckStatus::INFO, "LaunchAgent installed but not loaded", "",
                "autowhisper start"
            });
        } else {
            g.results.push_back({
                CheckStatus::WARN, "LaunchAgent not installed", "",
                "Run the install script (or `autowhisper install`)"
            });
        }
    }
    return g;
}

CheckGroup check_signing() {
    CheckGroup g;
    g.header = "Code signing";

    char path_buf[1024];
    uint32_t sz = sizeof(path_buf);
    if (_NSGetExecutablePath(path_buf, &sz) != 0) {
        g.results.push_back({
            CheckStatus::WARN, "Unable to resolve own executable path", "", ""
        });
        return g;
    }

    auto r = run_command({"codesign", "-dvvv", path_buf}, 5);
    // codesign writes to stderr on success.
    const std::string& out = r.stderr_str.empty() ? r.stdout_str : r.stderr_str;
    if (out.find("Developer ID Application") != std::string::npos) {
        g.results.push_back({
            CheckStatus::OK, "Signed with Developer ID Application", "", ""
        });
    } else if (out.find("adhoc") != std::string::npos ||
               out.find("Signature=adhoc") != std::string::npos) {
        g.results.push_back({
            CheckStatus::WARN, "Ad-hoc signed", "",
            "TCC grants may reset when the binary is rebuilt"
        });
    } else if (out.find("not signed") != std::string::npos ||
               out.find("code object is not signed") != std::string::npos) {
        g.results.push_back({
            CheckStatus::FAIL, "Binary is not signed", "",
            "codesign --force --sign - <path>   (for local dev use)"
        });
    } else {
        g.results.push_back({
            CheckStatus::INFO, "codesign reports:", out.substr(0, 120), ""
        });
    }
    return g;
}

} // namespace

std::vector<CheckGroup> platform_checks(bool /*fix*/) {
    std::vector<CheckGroup> out;
    out.push_back(check_permissions());
    out.push_back(check_metal_gpu());
    out.push_back(check_audio());
    out.push_back(check_service());
    out.push_back(check_signing());
    return out;
}

} // namespace autowhisper

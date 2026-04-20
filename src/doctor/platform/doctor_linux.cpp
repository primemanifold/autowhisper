#include "doctor/doctor.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace autowhisper {

namespace {

CheckGroup check_gpu(bool fix) {
    CheckGroup g;
    g.header = "NVIDIA GPU";

    if (!command_exists("nvidia-smi")) {
        g.results.push_back({
            CheckStatus::FAIL, "nvidia-smi not found", "",
            "sudo apt install nvidia-driver-535 && sudo reboot"
        });
        return g;
    }

    auto smi = run_command({"nvidia-smi"});
    if (smi.exit_code != 0) {
        if (smi.stderr_str.find("Driver/library version mismatch") != std::string::npos) {
            g.results.push_back({
                CheckStatus::FAIL, "NVIDIA driver/library version mismatch", "",
                "sudo reboot (or run: autowhisper doctor --fix)"
            });
            if (fix) {
                std::cout << "    Attempting fix: reloading nvidia modules...\n";
                run_command({"sudo", "rmmod", "nvidia_uvm", "nvidia_drm", "nvidia_modeset", "nvidia"});
                run_command({"sudo", "modprobe", "nvidia"});
            }
        } else {
            g.results.push_back({
                CheckStatus::FAIL, "nvidia-smi failed: " + smi.stderr_str, "", ""
            });
        }
        return g;
    }

    auto query = run_command({
        "nvidia-smi", "--query-gpu=name,driver_version,memory.total",
        "--format=csv,noheader"
    });

    if (query.exit_code == 0 && !query.stdout_str.empty()) {
        auto line = query.stdout_str;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

        size_t p1 = line.find(", ");
        size_t p2 = line.find(", ", p1 + 2);
        if (p1 != std::string::npos && p2 != std::string::npos) {
            g.results.push_back({ CheckStatus::INFO, "GPU:",    line.substr(0, p1), "" });
            g.results.push_back({ CheckStatus::INFO, "Driver:", line.substr(p1 + 2, p2 - p1 - 2), "" });
            g.results.push_back({ CheckStatus::INFO, "VRAM:",   line.substr(p2 + 2), "" });
        }
        g.results.push_back({ CheckStatus::OK, "NVIDIA GPU detected and working", "", "" });
    }
    return g;
}

CheckGroup check_audio() {
    CheckGroup g;
    g.header = "Audio";

    if (command_exists("arecord")) {
        auto devices = run_command({"arecord", "-l"});
        if (devices.exit_code == 0 && devices.stdout_str.find("card") != std::string::npos) {
            g.results.push_back({ CheckStatus::OK, "Audio capture devices found (ALSA)", "", "" });
            return g;
        }
    }
    if (command_exists("pactl")) {
        auto sources = run_command({"pactl", "list", "short", "sources"});
        if (sources.exit_code == 0 && !sources.stdout_str.empty()) {
            g.results.push_back({ CheckStatus::OK, "Audio capture devices found (PulseAudio)", "", "" });
            return g;
        }
    }
    g.results.push_back({
        CheckStatus::FAIL, "No audio input devices found", "",
        "Check microphone connection"
    });
    return g;
}

CheckGroup check_display() {
    CheckGroup g;
    g.header = "Display";

    const char* display = std::getenv("DISPLAY");
    if (!display || std::string(display).empty()) {
        g.results.push_back({
            CheckStatus::FAIL, "No DISPLAY environment variable", "",
            "Run from a graphical session, not SSH"
        });
        return g;
    }
    g.results.push_back({ CheckStatus::OK, "X11 display:", display, "" });

    auto xdpy = run_command({"xdpyinfo"}, 3);
    if (xdpy.exit_code == 0) {
        g.results.push_back({ CheckStatus::OK, "X11 connection successful", "", "" });
    } else {
        g.results.push_back({ CheckStatus::WARN, "X11 connection check failed (non-fatal)", "", "" });
    }
    return g;
}

CheckGroup check_tools(bool fix) {
    CheckGroup g;
    g.header = "Tools";

    struct Tool { const char* name; const char* desc; const char* install; };
    Tool tools[] = {
        {"xdotool", "Text injection",    "sudo apt install xdotool"},
        {"xclip",   "Clipboard access",  "sudo apt install xclip"},
    };

    for (const auto& tool : tools) {
        if (command_exists(tool.name)) {
            g.results.push_back({
                CheckStatus::OK, std::string(tool.desc) + " (" + tool.name + ")", "", ""
            });
        } else {
            g.results.push_back({
                CheckStatus::FAIL,
                std::string(tool.desc) + " (" + tool.name + ") not found", "",
                tool.install
            });
            if (fix) {
                std::cout << "    Installing " << tool.name << "...\n";
                run_command({"sudo", "apt", "install", "-y", tool.name});
            }
        }
    }
    return g;
}

CheckGroup check_service(bool fix) {
    CheckGroup g;
    g.header = "Service";

    auto active = run_command({"systemctl", "--user", "is-active", "autowhisper"});
    if (active.exit_code == 0) {
        auto status = active.stdout_str;
        while (!status.empty() && (status.back() == '\n' || status.back() == '\r')) status.pop_back();
        if (status == "active") {
            g.results.push_back({ CheckStatus::OK, "AutoWhisper service is running", "", "" });
            return g;
        }
    }

    const char* home = std::getenv("HOME");
    std::string service_path = std::string(home ? home : "/tmp") +
                               "/.config/systemd/user/autowhisper.service";

    if (!fs::exists(service_path)) {
        g.results.push_back({
            CheckStatus::WARN, "Service not installed", "",
            "Install the service file"
        });
    } else {
        g.results.push_back({ CheckStatus::INFO, "Service is stopped", "", "" });
        g.results.push_back({ CheckStatus::INFO, "Start with: autowhisper start", "", "" });
    }

    if (fix) {
        std::cout << "    Starting service...\n";
        run_command({"systemctl", "--user", "start", "autowhisper"});
    }
    return g;
}

} // namespace

std::vector<CheckGroup> platform_checks(bool fix) {
    std::vector<CheckGroup> out;
    out.push_back(check_gpu(fix));
    out.push_back(check_audio());
    out.push_back(check_display());
    out.push_back(check_tools(fix));
    out.push_back(check_service(fix));
    return out;
}

} // namespace autowhisper

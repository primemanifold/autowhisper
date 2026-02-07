#include "doctor/doctor.h"
#include "config/config.h"
#include "models/models.h"
#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace autowhisper {

namespace {

// ANSI color helpers
const char* GREEN = "\033[32m";
const char* YELLOW = "\033[33m";
const char* RED = "\033[31m";
const char* BLUE = "\033[34m";
const char* BOLD = "\033[1m";
const char* DIM = "\033[2m";
const char* RESET = "\033[0m";

struct DiagnosticResult {
    int passed = 0;
    int warnings = 0;
    int failed = 0;

    void ok(const std::string& msg) {
        std::cout << "  " << GREEN << "\xe2\x9c\x93" << RESET << " " << msg << "\n";
        passed++;
    }

    void warn(const std::string& msg, const std::string& fix = "") {
        std::cout << "  " << YELLOW << "!" << RESET << " " << msg << "\n";
        if (!fix.empty()) {
            std::cout << "    \xe2\x86\x92 " << DIM << fix << RESET << "\n";
        }
        warnings++;
    }

    void fail(const std::string& msg, const std::string& fix = "") {
        std::cout << "  " << RED << "\xe2\x9c\x97" << RESET << " " << msg << "\n";
        if (!fix.empty()) {
            std::cout << "    \xe2\x86\x92 " << DIM << fix << RESET << "\n";
        }
        failed++;
    }

    void info(const std::string& msg) {
        std::cout << "  " << BLUE << "\xe2\x80\xa2" << RESET << " " << msg << "\n";
    }
};

void header(const std::string& title) {
    std::cout << "\n" << BOLD << title << RESET << "\n";
    std::cout << std::string(title.size(), '-') << "\n";
}

void check_gpu(DiagnosticResult& result, bool fix) {
    header("NVIDIA GPU");

    if (!command_exists("nvidia-smi")) {
        result.fail("nvidia-smi not found",
                     "sudo apt install nvidia-driver-535 && sudo reboot");
        return;
    }

    auto smi = run_command({"nvidia-smi"});
    if (smi.exit_code != 0) {
        if (smi.stderr_str.find("Driver/library version mismatch") != std::string::npos) {
            result.fail("NVIDIA driver/library version mismatch",
                         "sudo reboot (or run: autowhisper doctor --fix)");
            if (fix) {
                std::cout << "    Attempting fix: reloading nvidia modules...\n";
                run_command({"sudo", "rmmod", "nvidia_uvm", "nvidia_drm", "nvidia_modeset", "nvidia"});
                run_command({"sudo", "modprobe", "nvidia"});
            }
        } else {
            result.fail("nvidia-smi failed: " + smi.stderr_str);
        }
        return;
    }

    auto query = run_command({
        "nvidia-smi", "--query-gpu=name,driver_version,memory.total",
        "--format=csv,noheader"
    });

    if (query.exit_code == 0 && !query.stdout_str.empty()) {
        // Parse "GPU Name, Driver, VRAM"
        auto line = query.stdout_str;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

        size_t p1 = line.find(", ");
        size_t p2 = line.find(", ", p1 + 2);
        if (p1 != std::string::npos && p2 != std::string::npos) {
            std::string gpu = line.substr(0, p1);
            std::string driver = line.substr(p1 + 2, p2 - p1 - 2);
            std::string vram = line.substr(p2 + 2);

            result.info("GPU: " + gpu);
            result.info("Driver: " + driver);
            result.info("VRAM: " + vram);
            result.ok("NVIDIA GPU detected and working");
        } else {
            result.ok("NVIDIA GPU detected");
        }
    }
}

void check_cuda(DiagnosticResult& result) {
    header("CUDA");

    // Check if whisper.cpp was built with CUDA by checking for nvidia-smi
    // (In the C++ version we don't have ctranslate2)
    auto smi = run_command({"nvidia-smi"});
    if (smi.exit_code == 0) {
        result.ok("NVIDIA GPU available for CUDA inference");
    } else {
        result.warn("No CUDA devices found", "Check nvidia-smi and driver installation");
    }
}

void check_audio(DiagnosticResult& result) {
    header("Audio");

    // Use miniaudio device enumeration
    // For doctor we just check if ALSA/PulseAudio are present
    bool has_audio = false;

    if (command_exists("arecord")) {
        auto devices = run_command({"arecord", "-l"});
        if (devices.exit_code == 0 && devices.stdout_str.find("card") != std::string::npos) {
            has_audio = true;
            result.ok("Audio capture devices found (ALSA)");
        }
    }

    if (!has_audio && command_exists("pactl")) {
        auto sources = run_command({"pactl", "list", "short", "sources"});
        if (sources.exit_code == 0 && !sources.stdout_str.empty()) {
            has_audio = true;
            result.ok("Audio capture devices found (PulseAudio)");
        }
    }

    if (!has_audio) {
        result.fail("No audio input devices found", "Check microphone connection");
    }
}

void check_display(DiagnosticResult& result) {
    header("Display");

    const char* display = std::getenv("DISPLAY");
    if (!display || std::string(display).empty()) {
        result.fail("No DISPLAY environment variable",
                     "Run from a graphical session, not SSH");
        return;
    }

    result.ok("X11 display: " + std::string(display));

    // Try xdpyinfo to verify connection
    auto xdpy = run_command({"xdpyinfo"}, 3);
    if (xdpy.exit_code == 0) {
        result.ok("X11 connection successful");
    } else {
        result.warn("X11 connection check failed (non-fatal)");
    }
}

void check_tools(DiagnosticResult& result, bool fix) {
    header("Tools");

    struct Tool {
        const char* name;
        const char* desc;
        const char* install;
    };

    Tool tools[] = {
        {"xdotool", "Text injection", "sudo apt install xdotool"},
        {"xclip", "Clipboard access", "sudo apt install xclip"},
    };

    for (const auto& tool : tools) {
        if (command_exists(tool.name)) {
            result.ok(std::string(tool.desc) + " (" + tool.name + ")");
        } else {
            result.fail(std::string(tool.desc) + " (" + tool.name + ") not found", tool.install);
            if (fix) {
                std::cout << "    Installing " << tool.name << "...\n";
                run_command({"sudo", "apt", "install", "-y", tool.name});
            }
        }
    }
}

void check_model(DiagnosticResult& result) {
    header("Model");

    try {
        std::string path = find_config_file();
        Config config = Config::load(path);

        result.info("Configured model: " + config.model.size);

        if (is_model_downloaded(config.model.size)) {
            result.ok("Model " + config.model.size + " is downloaded");
        } else {
            result.warn("Model " + config.model.size + " not found in cache",
                         "autowhisper model download " + config.model.size);
        }
    } catch (const std::exception& e) {
        result.warn("Model check failed: " + std::string(e.what()));
    }
}

void check_service(DiagnosticResult& result, bool fix) {
    header("Service");

    auto active = run_command({"systemctl", "--user", "is-active", "autowhisper"});

    if (active.exit_code == 0) {
        auto status = active.stdout_str;
        while (!status.empty() && (status.back() == '\n' || status.back() == '\r')) status.pop_back();
        if (status == "active") {
            result.ok("AutoWhisper service is running");
            return;
        }
    }

    // Check if service file exists
    const char* home = std::getenv("HOME");
    std::string service_path = std::string(home ? home : "/tmp") +
                               "/.config/systemd/user/autowhisper.service";

    if (!fs::exists(service_path)) {
        result.warn("Service not installed", "Install the service file");
    } else {
        result.info("Service is stopped");
        result.info("Start with: autowhisper start");
    }

    if (fix) {
        std::cout << "    Starting service...\n";
        run_command({"systemctl", "--user", "start", "autowhisper"});
    }
}

} // anonymous namespace

int run_diagnostics(bool fix) {
    std::cout << "\n" << BOLD << "AutoWhisper System Check" << RESET << "\n";
    std::cout << "========================\n";

    DiagnosticResult result;

    check_gpu(result, fix);
    check_cuda(result);
    check_audio(result);
    check_display(result);
    check_tools(result, fix);
    check_model(result);
    check_service(result, fix);

    // Summary
    header("Summary");
    std::cout << "\n";
    std::cout << "  " << GREEN << "Passed:  " << RESET << " " << result.passed << "\n";
    std::cout << "  " << YELLOW << "Warnings:" << RESET << " " << result.warnings << "\n";
    std::cout << "  " << RED << "Failed:  " << RESET << " " << result.failed << "\n";
    std::cout << "\n";

    if (result.failed == 0) {
        if (result.warnings == 0) {
            std::cout << GREEN << "System is fully ready for AutoWhisper!" << RESET << "\n";
        } else {
            std::cout << YELLOW << "System is ready with some warnings." << RESET << "\n";
        }
        std::cout << "\nStart with: autowhisper start\n";
    } else {
        std::cout << RED << "Please fix the issues above." << RESET << "\n";
        if (!fix) {
            std::cout << "Try: autowhisper doctor --fix\n";
        }
    }

    std::cout << "\n";
    return result.failed;
}

} // namespace autowhisper

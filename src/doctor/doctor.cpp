#include "doctor/doctor.h"
#include "config/config.h"
#include "models/models.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace autowhisper {

namespace {

const char* GREEN  = "\033[32m";
const char* YELLOW = "\033[33m";
const char* RED    = "\033[31m";
const char* BLUE   = "\033[34m";
const char* BOLD   = "\033[1m";
const char* DIM    = "\033[2m";
const char* RESET  = "\033[0m";

void print_result(const CheckResult& r) {
    switch (r.status) {
        case CheckStatus::OK:
            std::cout << "  " << GREEN << "\xe2\x9c\x93" << RESET
                      << " " << r.label;
            break;
        case CheckStatus::WARN:
            std::cout << "  " << YELLOW << "!" << RESET << " " << r.label;
            break;
        case CheckStatus::FAIL:
            std::cout << "  " << RED << "\xe2\x9c\x97" << RESET
                      << " " << r.label;
            break;
        case CheckStatus::INFO:
            std::cout << "  " << BLUE << "\xe2\x80\xa2" << RESET
                      << " " << r.label;
            break;
    }
    if (!r.message.empty()) std::cout << " " << r.message;
    std::cout << "\n";
    if (!r.fix.empty()) {
        std::cout << "    \xe2\x86\x92 " << DIM << r.fix << RESET << "\n";
    }
}

void print_group(const CheckGroup& g) {
    std::cout << "\n" << BOLD << g.header << RESET << "\n";
    std::cout << std::string(g.header.size(), '-') << "\n";
    for (const auto& r : g.results) print_result(r);
}

} // namespace

std::vector<CheckGroup> common_checks() {
    std::vector<CheckGroup> out;

    // Model check — looks at config + downloaded-model cache.
    CheckGroup model_group;
    model_group.header = "Model";
    try {
        std::string path = find_config_file();
        Config config = Config::load(path);

        model_group.results.push_back({
            CheckStatus::INFO, "Configured model:", config.model.size, ""
        });
        if (is_model_downloaded(config.model.size)) {
            model_group.results.push_back({
                CheckStatus::OK,
                "Model " + config.model.size + " is downloaded", "", ""
            });
        } else {
            model_group.results.push_back({
                CheckStatus::WARN,
                "Model " + config.model.size + " not found in cache", "",
                "autowhisper model download " + config.model.size
            });
        }
    } catch (const std::exception& e) {
        model_group.results.push_back({
            CheckStatus::WARN,
            std::string("Model check failed: ") + e.what(), "", ""
        });
    }
    out.push_back(std::move(model_group));

    return out;
}

int render_and_exit(const std::vector<CheckGroup>& groups) {
    int passed = 0, warnings = 0, failed = 0;
    for (const auto& g : groups) {
        print_group(g);
        for (const auto& r : g.results) {
            switch (r.status) {
                case CheckStatus::OK:   ++passed;    break;
                case CheckStatus::WARN: ++warnings;  break;
                case CheckStatus::FAIL: ++failed;    break;
                case CheckStatus::INFO: break;
            }
        }
    }

    std::cout << "\n" << BOLD << "Summary" << RESET << "\n-------\n";
    std::cout << "  " << GREEN  << "Passed:   " << RESET << passed   << "\n";
    std::cout << "  " << YELLOW << "Warnings: " << RESET << warnings << "\n";
    std::cout << "  " << RED    << "Failed:   " << RESET << failed   << "\n\n";

    if (failed == 0) {
        if (warnings == 0) {
            std::cout << GREEN << "System is fully ready for AutoWhisper!"
                      << RESET << "\n";
        } else {
            std::cout << YELLOW << "System is ready with some warnings."
                      << RESET << "\n";
        }
        std::cout << "\nStart with: autowhisper start\n";
    } else {
        std::cout << RED << "Please fix the issues above." << RESET << "\n";
    }
    std::cout << "\n";
    return failed;
}

int run_diagnostics(bool fix) {
    std::cout << "\n" << BOLD << "AutoWhisper System Check" << RESET << "\n";
    std::cout << "========================\n";

    std::vector<CheckGroup> all;
    auto platform = platform_checks(fix);
    auto common   = common_checks();
    all.reserve(platform.size() + common.size());
    for (auto& g : platform) all.push_back(std::move(g));
    for (auto& g : common)   all.push_back(std::move(g));

    return render_and_exit(all);
}

} // namespace autowhisper

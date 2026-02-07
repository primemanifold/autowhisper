#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#else
#include <windows.h>
#endif

namespace autowhisper {

#ifndef _WIN32

ProcessResult run_command(const std::vector<std::string>& args, int timeout_seconds) {
    ProcessResult result;
    if (args.empty()) return result;

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        spdlog::warn("Failed to create pipes");
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::warn("Fork failed");
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::vector<char*> c_args;
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    // Parent
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Read with poll for timeout
    struct pollfd fds[2];
    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN;

    int timeout_ms = timeout_seconds * 1000;
    std::array<char, 4096> buf;

    bool stdout_done = false;
    bool stderr_done = false;

    while (!stdout_done || !stderr_done) {
        int nfds = 0;
        struct pollfd active_fds[2];
        if (!stdout_done) { active_fds[nfds] = fds[0]; nfds++; }
        if (!stderr_done) { active_fds[nfds] = fds[1]; nfds++; }

        int ret = poll(active_fds, nfds, timeout_ms);
        if (ret <= 0) {
            // Timeout or error
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            result.exit_code = -1;
            result.stderr_str = "timeout";
            return result;
        }

        for (int i = 0; i < nfds; i++) {
            if (active_fds[i].revents & POLLIN) {
                ssize_t n = read(active_fds[i].fd, buf.data(), buf.size());
                if (n > 0) {
                    if (active_fds[i].fd == stdout_pipe[0])
                        result.stdout_str.append(buf.data(), n);
                    else
                        result.stderr_str.append(buf.data(), n);
                } else {
                    if (active_fds[i].fd == stdout_pipe[0]) stdout_done = true;
                    else stderr_done = true;
                }
            }
            if (active_fds[i].revents & (POLLHUP | POLLERR)) {
                // Read remaining data
                ssize_t n;
                while ((n = read(active_fds[i].fd, buf.data(), buf.size())) > 0) {
                    if (active_fds[i].fd == stdout_pipe[0])
                        result.stdout_str.append(buf.data(), n);
                    else
                        result.stderr_str.append(buf.data(), n);
                }
                if (active_fds[i].fd == stdout_pipe[0]) stdout_done = true;
                else stderr_done = true;
            }
        }
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }

    return result;
}

ProcessResult run_command_with_input(const std::vector<std::string>& args,
                                     const std::string& input,
                                     int timeout_seconds) {
    ProcessResult result;
    if (args.empty()) return result;

    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::vector<char*> c_args;
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Write input
    write(stdin_pipe[1], input.data(), input.size());
    close(stdin_pipe[1]);

    // Read output
    std::array<char, 4096> buf;
    ssize_t n;
    while ((n = read(stdout_pipe[0], buf.data(), buf.size())) > 0) {
        result.stdout_str.append(buf.data(), n);
    }
    while ((n = read(stderr_pipe[0], buf.data(), buf.size())) > 0) {
        result.stderr_str.append(buf.data(), n);
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }

    return result;
}

int run_passthrough(const std::vector<std::string>& args) {
    if (args.empty()) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        std::vector<char*> c_args;
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#else // _WIN32

ProcessResult run_command(const std::vector<std::string>& args, int timeout_seconds) {
    ProcessResult result;
    // Windows implementation placeholder
    result.exit_code = -1;
    result.stderr_str = "not implemented on Windows";
    return result;
}

ProcessResult run_command_with_input(const std::vector<std::string>& args,
                                     const std::string& input,
                                     int timeout_seconds) {
    ProcessResult result;
    result.exit_code = -1;
    return result;
}

int run_passthrough(const std::vector<std::string>& args) {
    return -1;
}

#endif

bool command_exists(const std::string& name) {
#ifndef _WIN32
    auto result = run_command({"which", name}, 5);
    return result.exit_code == 0;
#else
    auto result = run_command({"where", name}, 5);
    return result.exit_code == 0;
#endif
}

} // namespace autowhisper

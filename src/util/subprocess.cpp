#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#else
#include <windows.h>
#endif

namespace autowhisper {

#ifndef _WIN32

namespace {

inline void close_fd(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

inline bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

}  // namespace

ProcessResult run_command(const std::vector<std::string>& args, int timeout_seconds) {
    ProcessResult result;
    if (args.empty()) return result;

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0) {
        spdlog::warn("Failed to create pipes");
        return result;
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
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

    if (pipe(stdin_pipe) != 0) {
        return result;
    }
    if (pipe(stdout_pipe) != 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        return result;
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
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

    close_fd(stdin_pipe[0]);
    close_fd(stdout_pipe[1]);
    close_fd(stderr_pipe[1]);

    set_nonblocking(stdin_pipe[1]);
    set_nonblocking(stdout_pipe[0]);
    set_nonblocking(stderr_pipe[0]);

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout_seconds);
    std::array<char, 4096> buf;

    size_t written = 0;
    const size_t total = input.size();
    bool stdin_done = (total == 0);
    bool stdout_done = false;
    bool stderr_done = false;
    bool child_exited = false;
    int child_status = 0;

    if (stdin_done) {
        close_fd(stdin_pipe[1]);
    }

    while (!stdin_done || !stdout_done || !stderr_done) {
        // Once stdin is written, use short poll intervals so we can
        // check whether the child has exited.  Programs like xclip fork
        // a background process that keeps our pipe FDs open; without
        // this we'd block until the full timeout.
        int poll_ms;
        if (stdin_done) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, 0);
                close_fd(stdin_pipe[1]);
                close_fd(stdout_pipe[0]);
                close_fd(stderr_pipe[0]);
                result.exit_code = -1;
                result.stderr_str = "timeout";
                return result;
            }
            poll_ms = std::min<int>(50, static_cast<int>(remaining));
        } else {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            poll_ms = std::max<int>(0, static_cast<int>(remaining));
        }

        struct pollfd active_fds[3];
        int nfds = 0;

        if (!stdin_done && stdin_pipe[1] >= 0) {
            active_fds[nfds].fd = stdin_pipe[1];
            active_fds[nfds].events = POLLOUT;
            active_fds[nfds].revents = 0;
            nfds++;
        }
        if (!stdout_done && stdout_pipe[0] >= 0) {
            active_fds[nfds].fd = stdout_pipe[0];
            active_fds[nfds].events = POLLIN;
            active_fds[nfds].revents = 0;
            nfds++;
        }
        if (!stderr_done && stderr_pipe[0] >= 0) {
            active_fds[nfds].fd = stderr_pipe[0];
            active_fds[nfds].events = POLLIN;
            active_fds[nfds].revents = 0;
            nfds++;
        }

        if (nfds == 0) break;

        int ret = poll(active_fds, nfds, poll_ms);
        if (ret < 0 && errno != EINTR) {
            break;
        }

        // Process any available data
        for (int i = 0; i < nfds && ret > 0; i++) {
            int fd = active_fds[i].fd;
            short revents = active_fds[i].revents;

            if (fd == stdin_pipe[1]) {
                if (revents & (POLLOUT | POLLERR | POLLHUP)) {
                    if (written < total) {
                        const size_t chunk = std::min<size_t>(4096, total - written);
                        ssize_t n = write(stdin_pipe[1], input.data() + written, chunk);
                        if (n > 0) {
                            written += static_cast<size_t>(n);
                        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                            continue;
                        } else {
                            written = total;
                        }
                    }

                    if (written >= total) {
                        close_fd(stdin_pipe[1]);
                        stdin_done = true;
                    }
                }
                continue;
            }

            if (revents & (POLLIN | POLLERR | POLLHUP)) {
                for (;;) {
                    ssize_t n = read(fd, buf.data(), buf.size());
                    if (n > 0) {
                        if (fd == stdout_pipe[0]) {
                            result.stdout_str.append(buf.data(), n);
                        } else {
                            result.stderr_str.append(buf.data(), n);
                        }
                        continue;
                    }

                    if (n == 0) {
                        if (fd == stdout_pipe[0]) {
                            close_fd(stdout_pipe[0]);
                            stdout_done = true;
                        } else {
                            close_fd(stderr_pipe[0]);
                            stderr_done = true;
                        }
                        break;
                    }

                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }

                    if (fd == stdout_pipe[0]) {
                        close_fd(stdout_pipe[0]);
                        stdout_done = true;
                    } else {
                        close_fd(stderr_pipe[0]);
                        stderr_done = true;
                    }
                    break;
                }
            }
        }

        // Check if child has exited — if so, we're done even if a
        // grandchild still holds our pipe FDs open.
        if (stdin_done && !child_exited) {
            pid_t wpid = waitpid(pid, &child_status, WNOHANG);
            if (wpid == pid) {
                child_exited = true;
                if (WIFEXITED(child_status)) {
                    result.exit_code = WEXITSTATUS(child_status);
                }
                close_fd(stdin_pipe[1]);
                close_fd(stdout_pipe[0]);
                close_fd(stderr_pipe[0]);
                return result;
            }
        }
    }

    close_fd(stdin_pipe[1]);
    close_fd(stdout_pipe[0]);
    close_fd(stderr_pipe[0]);

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

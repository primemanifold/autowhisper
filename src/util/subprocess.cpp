#include "util/subprocess.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>

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

inline void terminate_child(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}

inline bool wait_for_child_with_deadline(pid_t pid,
                                         const std::chrono::steady_clock::time_point& deadline,
                                         int* status_out) {
    for (;;) {
        int status = 0;
        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == pid) {
            if (status_out) *status_out = status;
            return true;
        }
        if (rc < 0) {
            if (errno == EINTR) continue;
            return false;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        usleep(10000);
    }
}

inline void append_bounded(std::string& destination, const char* data,
                           std::size_t size, std::size_t limit,
                           std::size_t& captured, bool& truncated) {
    const std::size_t remaining = captured < limit ? limit - captured : 0;
    const std::size_t accepted = std::min(size, remaining);
    if (accepted > 0) {
        destination.append(data, accepted);
        captured += accepted;
    }
    if (accepted < size) truncated = true;
}

}  // namespace

ProcessResult run_command(const std::vector<std::string>& args, int timeout_seconds,
                          std::size_t max_output_bytes) {
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

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout_seconds);
    std::array<char, 4096> buf;

    bool stdout_done = false;
    bool stderr_done = false;
    std::size_t captured = 0;

    while (!stdout_done || !stderr_done) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            terminate_child(pid);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            result.exit_code = -1;
            result.stderr_str = "timeout";
            return result;
        }

        int nfds = 0;
        struct pollfd active_fds[2];
        if (!stdout_done) { active_fds[nfds] = fds[0]; nfds++; }
        if (!stderr_done) { active_fds[nfds] = fds[1]; nfds++; }

        int ret = poll(active_fds, nfds, static_cast<int>(remaining));
        if (ret == 0) {
            terminate_child(pid);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            result.exit_code = -1;
            result.stderr_str = "timeout";
            return result;
        }
        if (ret < 0) {
            if (errno == EINTR) continue;
            terminate_child(pid);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            result.exit_code = -1;
            result.stderr_str = "poll_error";
            return result;
        }

        for (int i = 0; i < nfds; i++) {
            if (active_fds[i].revents & POLLIN) {
                ssize_t n = read(active_fds[i].fd, buf.data(), buf.size());
                if (n > 0) {
                    if (active_fds[i].fd == stdout_pipe[0]) {
                        append_bounded(result.stdout_str, buf.data(), static_cast<std::size_t>(n),
                                       max_output_bytes, captured, result.output_truncated);
                    } else {
                        append_bounded(result.stderr_str, buf.data(), static_cast<std::size_t>(n),
                                       max_output_bytes, captured, result.output_truncated);
                    }
                } else {
                    if (active_fds[i].fd == stdout_pipe[0]) stdout_done = true;
                    else stderr_done = true;
                }
            }
            if (active_fds[i].revents & (POLLHUP | POLLERR)) {
                // Read remaining data
                ssize_t n;
                while ((n = read(active_fds[i].fd, buf.data(), buf.size())) > 0) {
                    if (active_fds[i].fd == stdout_pipe[0]) {
                        append_bounded(result.stdout_str, buf.data(), static_cast<std::size_t>(n),
                                       max_output_bytes, captured, result.output_truncated);
                    } else {
                        append_bounded(result.stderr_str, buf.data(), static_cast<std::size_t>(n),
                                       max_output_bytes, captured, result.output_truncated);
                    }
                }
                if (active_fds[i].fd == stdout_pipe[0]) stdout_done = true;
                else stderr_done = true;
            }
        }
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status = 0;
    if (!wait_for_child_with_deadline(pid, deadline, &status)) {
        terminate_child(pid);
        result.exit_code = -1;
        result.stderr_str = "timeout";
        return result;
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }

    return result;
}

ProcessResult run_command_with_input(const std::vector<std::string>& args,
                                     const std::string& input,
                                     int timeout_seconds,
                                     std::size_t max_output_bytes) {
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
    std::size_t captured = 0;

    if (stdin_done) {
        close_fd(stdin_pipe[1]);
    }

    while (!stdin_done || !stdout_done || !stderr_done) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            terminate_child(pid);
            close_fd(stdin_pipe[1]);
            close_fd(stdout_pipe[0]);
            close_fd(stderr_pipe[0]);
            result.exit_code = -1;
            result.stderr_str = "timeout";
            return result;
        }

        // Once stdin is written, use short poll intervals so we can
        // check whether the child has exited.  Programs like xclip fork
        // a background process that keeps our pipe FDs open; without
        // this we'd block until the full timeout.
        int poll_ms = stdin_done
            ? std::min<int>(50, static_cast<int>(remaining))
            : static_cast<int>(remaining);

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
            terminate_child(pid);
            close_fd(stdin_pipe[1]);
            close_fd(stdout_pipe[0]);
            close_fd(stderr_pipe[0]);
            result.exit_code = -1;
            result.stderr_str = "poll_error";
            return result;
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
                            append_bounded(result.stdout_str, buf.data(), static_cast<std::size_t>(n),
                                           max_output_bytes, captured, result.output_truncated);
                        } else {
                            append_bounded(result.stderr_str, buf.data(), static_cast<std::size_t>(n),
                                           max_output_bytes, captured, result.output_truncated);
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

        // Check if child has exited — if so, drain remaining pipe
        // data and return.  This prevents hanging when programs like
        // xclip fork a background process that inherits our pipe FDs.
        if (stdin_done && !child_exited) {
            pid_t wpid = waitpid(pid, &child_status, WNOHANG);
            if (wpid == pid) {
                child_exited = true;
                if (WIFEXITED(child_status)) {
                    result.exit_code = WEXITSTATUS(child_status);
                }
                // Drain any remaining data from pipes
                ssize_t n;
                if (stdout_pipe[0] >= 0) {
                    while ((n = read(stdout_pipe[0], buf.data(), buf.size())) > 0) {
                        append_bounded(result.stdout_str, buf.data(), static_cast<std::size_t>(n),
                                       max_output_bytes, captured, result.output_truncated);
                    }
                }
                if (stderr_pipe[0] >= 0) {
                    while ((n = read(stderr_pipe[0], buf.data(), buf.size())) > 0) {
                        append_bounded(result.stderr_str, buf.data(), static_cast<std::size_t>(n),
                                       max_output_bytes, captured, result.output_truncated);
                    }
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

    int status = 0;
    if (!wait_for_child_with_deadline(pid, deadline, &status)) {
        terminate_child(pid);
        result.exit_code = -1;
        result.stderr_str = "timeout";
        return result;
    }
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

namespace {

void append_bounded_windows(std::string& destination, const char* data,
                            std::size_t size, std::size_t limit,
                            std::size_t& captured, bool& truncated) {
    const std::size_t remaining = captured < limit ? limit - captured : 0;
    const std::size_t accepted = std::min(size, remaining);
    if (accepted > 0) {
        destination.append(data, accepted);
        captured += accepted;
    }
    if (accepted < size) truncated = true;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         value.data(), static_cast<int>(value.size()),
                                         nullptr, 0);
    if (size <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            value.data(), static_cast<int>(value.size()),
                            wide.data(), size) <= 0) {
        return {};
    }
    return wide;
}

std::wstring quote_windows_arg(const std::wstring& value) {
    if (!value.empty() && value.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return value;
    }

    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'\"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

ProcessResult run_process_windows(const std::vector<std::string>& args,
                                  const std::string& input,
                                  int timeout_seconds,
                                  std::size_t max_output_bytes) {
    ProcessResult result;
    if (args.empty()) return result;

    std::wstring command_line;
    for (const auto& arg : args) {
        std::wstring wide = utf8_to_wide(arg);
        if (wide.empty() && !arg.empty()) {
            result.stderr_str = "invalid_utf8_argument";
            return result;
        }
        if (!command_line.empty()) command_line.push_back(L' ');
        command_line += quote_windows_arg(wide);
    }
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE child_stdin_read = nullptr;
    HANDLE parent_stdin_write = nullptr;
    HANDLE parent_stdout_read = nullptr;
    HANDLE child_stdout_write = nullptr;
    HANDLE parent_stderr_read = nullptr;
    HANDLE child_stderr_write = nullptr;

    auto close_handle = [](HANDLE& handle) {
        if (handle) {
            CloseHandle(handle);
            handle = nullptr;
        }
    };
    auto close_pipes = [&]() {
        close_handle(child_stdin_read);
        close_handle(parent_stdin_write);
        close_handle(parent_stdout_read);
        close_handle(child_stdout_write);
        close_handle(parent_stderr_read);
        close_handle(child_stderr_write);
    };

    if (!CreatePipe(&child_stdin_read, &parent_stdin_write, &security, 0) ||
        !CreatePipe(&parent_stdout_read, &child_stdout_write, &security, 0) ||
        !CreatePipe(&parent_stderr_read, &child_stderr_write, &security, 0)) {
        close_pipes();
        result.stderr_str = "pipe_error";
        return result;
    }
    SetHandleInformation(parent_stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(parent_stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(parent_stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_stdin_read;
    startup.hStdOutput = child_stdout_write;
    startup.hStdError = child_stderr_write;
    PROCESS_INFORMATION process{};

    const BOOL created = CreateProcessW(
        nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    if (!created) {
        const DWORD error = GetLastError();
        close_pipes();
        result.stderr_str = "create_process_failed:" + std::to_string(error);
        return result;
    }

    close_handle(child_stdin_read);
    close_handle(child_stdout_write);
    close_handle(child_stderr_write);

    std::mutex capture_mutex;
    std::size_t captured = 0;
    auto reader = [&](HANDLE handle, std::string& destination) {
        std::array<char, 4096> buffer{};
        DWORD count = 0;
        while (ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) &&
               count > 0) {
            std::lock_guard<std::mutex> lock(capture_mutex);
            append_bounded_windows(destination, buffer.data(), static_cast<std::size_t>(count),
                                   max_output_bytes, captured, result.output_truncated);
        }
        CloseHandle(handle);
    };
    auto writer = [&](HANDLE handle) {
        std::size_t offset = 0;
        while (offset < input.size()) {
            DWORD written = 0;
            const DWORD chunk = static_cast<DWORD>(
                std::min<std::size_t>(4096, input.size() - offset));
            if (!WriteFile(handle, input.data() + offset, chunk, &written, nullptr) ||
                written == 0) {
                break;
            }
            offset += written;
        }
        CloseHandle(handle);
    };

    std::thread stdout_thread(reader, parent_stdout_read, std::ref(result.stdout_str));
    std::thread stderr_thread(reader, parent_stderr_read, std::ref(result.stderr_str));
    std::thread stdin_thread(writer, parent_stdin_write);
    parent_stdout_read = nullptr;
    parent_stderr_read = nullptr;
    parent_stdin_write = nullptr;

    const DWORD wait_ms = timeout_seconds <= 0
        ? 0
        : static_cast<DWORD>(std::min<long long>(
              static_cast<long long>(timeout_seconds) * 1000,
              static_cast<long long>(INFINITE - 1)));
    const DWORD wait_result = WaitForSingleObject(process.hProcess, wait_ms);
    const bool timed_out = wait_result == WAIT_TIMEOUT;
    const bool wait_failed = wait_result != WAIT_OBJECT_0 && !timed_out;
    if (timed_out || wait_failed) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
    }

    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    stdin_thread.join();
    stdout_thread.join();
    stderr_thread.join();

    if (timed_out) {
        result.exit_code = -1;
        result.stderr_str = "timeout";
    } else if (wait_result == WAIT_OBJECT_0) {
        result.exit_code = static_cast<int>(exit_code);
    } else if (wait_failed) {
        result.exit_code = -1;
        result.stderr_str = "wait_error";
    }
    return result;
}

}  // namespace

ProcessResult run_command(const std::vector<std::string>& args, int timeout_seconds,
                          std::size_t max_output_bytes) {
    return run_process_windows(args, "", timeout_seconds, max_output_bytes);
}

ProcessResult run_command_with_input(const std::vector<std::string>& args,
                                     const std::string& input,
                                     int timeout_seconds,
                                     std::size_t max_output_bytes) {
    return run_process_windows(args, input, timeout_seconds, max_output_bytes);
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

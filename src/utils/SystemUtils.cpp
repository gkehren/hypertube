#include "SystemUtils.hpp"
#include "Logger.hpp"
#include <cstdlib>
#include <thread>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <filesystem>
#include <utility>
#include <initializer_list>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>
#include <cerrno>
#include <cstring>
extern char **environ;
#endif

namespace Utils {
    namespace SystemUtils {

        namespace {
            Result validateOpenPath(OpenOperationKind kind, const std::string &path) {
                std::error_code error;
                if (path.empty() || (kind == OpenOperationKind::Explorer
                    ? !std::filesystem::exists(path, error)
                    : !std::filesystem::is_regular_file(path, error)))
                    return Result::Failure("Cannot open path because it does not exist: " + path, ResultCode::NotFound);
                return Result::Success();
            }

            Result launchPlatformProcess(OpenOperationKind kind, const std::string &path) {
#ifdef _WIN32
                HINSTANCE result = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32)
                    return Result::Failure("The operating system could not open the requested path", ResultCode::Unavailable, true);
                return Result::Success();
#elif defined(__APPLE__) || defined(__linux__)
#ifdef __APPLE__
                const char* cmd = "open";
#else
                const char* cmd = "xdg-open";
#endif
                char *const argv[] = {const_cast<char *>(cmd), const_cast<char *>(path.c_str()), nullptr};
                pid_t pid = -1;
                const int status = posix_spawnp(&pid, cmd, nullptr, nullptr, argv, environ);
                if (status != 0)
                    return Result::Failure("Unable to launch the operating system opener", ResultCode::Unavailable, true);
                int waitStatus = 0;
                if (waitpid(pid, &waitStatus, 0) < 0 || !WIFEXITED(waitStatus) || WEXITSTATUS(waitStatus) != 0)
                    return Result::Failure("The operating system opener failed", ResultCode::Unavailable, true);
                return Result::Success();
#else
                return Result::Failure("Opening paths is not supported on this platform", ResultCode::Unavailable);
#endif
            }

#ifndef _WIN32
            Result copyWithProcess(const char *program, std::initializer_list<const char *> arguments,
                const std::string &text) {
                int pipes[2] = {-1, -1};
                if (pipe(pipes) != 0)
                    return Result::Failure("Unable to create a clipboard pipe", ResultCode::Unavailable, true);
                std::vector<std::string> storage{program};
                for (const auto *argument : arguments)
                    storage.emplace_back(argument);
                std::vector<char *> argv;
                for (auto &argument : storage)
                    argv.push_back(argument.data());
                argv.push_back(nullptr);
                posix_spawn_file_actions_t actions;
                if (posix_spawn_file_actions_init(&actions) != 0) {
                    close(pipes[0]);
                    close(pipes[1]);
                    return Result::Failure("Unable to initialize clipboard process", ResultCode::Unavailable, true);
                }
                posix_spawn_file_actions_adddup2(&actions, pipes[0], STDIN_FILENO);
                posix_spawn_file_actions_addclose(&actions, pipes[1]);
                pid_t pid = -1;
                const int error = posix_spawnp(&pid, program, &actions, nullptr, argv.data(), environ);
                posix_spawn_file_actions_destroy(&actions);
                close(pipes[0]);
                if (error != 0) {
                    close(pipes[1]);
                    return Result::Failure(error == ENOENT ? "No clipboard backend is available"
                        : std::string("Unable to launch clipboard backend: ") + std::strerror(error),
                        ResultCode::Unavailable, true);
                }
                const char *data = text.data();
                std::size_t remaining = text.size();
                while (remaining > 0) {
                    const ssize_t written = write(pipes[1], data, remaining);
                    if (written <= 0) {
                        close(pipes[1]);
                        waitpid(pid, nullptr, 0);
                        return Result::Failure("Unable to write to the clipboard backend", ResultCode::Unavailable, true);
                    }
                    data += written;
                    remaining -= static_cast<std::size_t>(written);
                }
                close(pipes[1]);
                int status = 0;
                if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                    return Result::Failure("The clipboard backend rejected the text", ResultCode::Unavailable, true);
                return Result::Success();
            }
#endif
        }

        SystemOpener::SystemOpener(std::size_t pendingLimit, Executor processExecutor)
            : maxPending(std::max<std::size_t>(1, pendingLimit)), executor(std::move(processExecutor)) {
            worker = std::thread(&SystemOpener::workerLoop, this);
        }

        SystemOpener::~SystemOpener() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                stopping = true;
            }
            condition.notify_one();
            if (worker.joinable())
                worker.join();
        }

        Result SystemOpener::enqueueExplorer(const std::string &path, std::uint64_t *id) {
            return enqueue(OpenOperationKind::Explorer, path, id);
        }

        Result SystemOpener::enqueuePreview(const std::string &path, std::uint64_t *id) {
            return enqueue(OpenOperationKind::Preview, path, id);
        }

        Result SystemOpener::enqueue(OpenOperationKind kind, const std::string &path, std::uint64_t *id) {
            Result validation = validateOpenPath(kind, path);
            if (!validation)
                return validation;
            std::lock_guard<std::mutex> lock(mutex);
            if (stopping)
                return Result::Failure("System opener is shutting down", ResultCode::Unavailable);
			if (requests.size() + results.size() >= maxPending)
                return Result::Failure("Too many pending open operations", ResultCode::Busy, true);
            const std::uint64_t requestId = nextId++;
            requests.push_back({requestId, kind, path});
            if (id)
                *id = requestId;
            condition.notify_one();
            return Result::Success();
        }

        Result SystemOpener::execute(const Request &request) {
            Result result = executor ? executor(request.kind, request.path)
                : launchPlatformProcess(request.kind, request.path);
            if (!result)
                Utils::Logger::error("ui", "Failed to open path: " + result.message);
            return result;
        }

        std::vector<OpenOperationResult> SystemOpener::drainResults() {
            std::deque<OpenOperationResult> pending;
            {
                std::lock_guard<std::mutex> lock(mutex);
                pending.swap(results);
            }
            return std::vector<OpenOperationResult>(std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
        }

        void SystemOpener::workerLoop() {
            while (true) {
                Request request{};
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    condition.wait(lock, [this] { return stopping || !requests.empty(); });
                    if (requests.empty() && stopping)
                        return;
                    request = std::move(requests.front());
                    requests.pop_front();
                }
                OpenOperationResult operationResult{request.id, request.kind, execute(request)};
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    results.push_back(std::move(operationResult));
                }
            }
        }

        Result openFileExplorer(const std::string& path) {
            Result validation = validateOpenPath(OpenOperationKind::Explorer, path);
            return validation ? launchPlatformProcess(OpenOperationKind::Explorer, path) : validation;
        }

        bool getLocalTime(const std::time_t& time, std::tm& result) {
#ifdef _WIN32
            // Windows localtime_s returns 0 on success.
            // Signature: errno_t localtime_s(struct tm* _tm, const time_t *time);
            return localtime_s(&result, &time) == 0;
#else
            // POSIX localtime_r returns pointer to result on success, NULL on error.
            // Signature: struct tm *localtime_r(const time_t *timep, struct tm *result);
            return localtime_r(&time, &result) != nullptr;
#endif
        }

        bool isPreviewableFile(const std::string& filename) {
            // Extract file extension
            size_t dotPos = filename.find_last_of('.');
            if (dotPos == std::string::npos) {
                return false; // No extension
            }

            std::string ext = filename.substr(dotPos + 1);
            // Convert to lowercase for comparison
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return std::tolower(c); });

            // Video formats
            static const char* videoExts[] = {
                "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "3gp", "ogv"
            };
            for (const char* videoExt : videoExts) {
                if (ext == videoExt) return true;
            }

            // Audio formats
            static const char* audioExts[] = {
                "mp3", "wav", "flac", "aac", "ogg", "wma", "m4a", "opus", "ape", "alac"
            };
            for (const char* audioExt : audioExts) {
                if (ext == audioExt) return true;
            }

            // Image formats
            static const char* imageExts[] = {
                "jpg", "jpeg", "png", "gif", "bmp", "webp", "svg", "ico", "tiff", "tif"
            };
            for (const char* imageExt : imageExts) {
                if (ext == imageExt) return true;
            }

            // Text formats
            static const char* textExts[] = {
                "txt", "log", "md", "json", "xml", "html", "css", "js", "cpp", "hpp", "c", "h", "py", "java", "pdf"
            };
            for (const char* textExt : textExts) {
                if (ext == textExt) return true;
            }

            return false;
        }

        Result openFilePreview(const std::string& filePath) {
            Result validation = validateOpenPath(OpenOperationKind::Preview, filePath);
            return validation ? launchPlatformProcess(OpenOperationKind::Preview, filePath) : validation;
        }

        Result copyToClipboard(const std::string &text) {
            if (text.empty())
                return Result::Failure("Nothing to copy", ResultCode::InvalidInput);
#ifdef _WIN32
            if (!OpenClipboard(nullptr))
                return Result::Failure("Unable to open the Windows clipboard", ResultCode::Unavailable, true);
            EmptyClipboard();
            const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                static_cast<int>(text.size()), nullptr, 0);
            if (wideLength <= 0) {
                CloseClipboard();
                return Result::Failure("Magnet URI is not valid UTF-8", ResultCode::InvalidInput);
            }
            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wideLength + 1) * sizeof(wchar_t));
            if (!memory) {
                CloseClipboard();
                return Result::Failure("Unable to allocate clipboard memory", ResultCode::Unavailable, true);
            }
            auto *target = static_cast<wchar_t *>(GlobalLock(memory));
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                target, wideLength);
            target[wideLength] = L'\0';
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_UNICODETEXT, memory)) {
                GlobalFree(memory);
                CloseClipboard();
                return Result::Failure("Unable to set the Windows clipboard", ResultCode::Unavailable, true);
            }
            CloseClipboard();
            return Result::Success();
#elif defined(__APPLE__)
            return copyWithProcess("pbcopy", {}, text);
#elif defined(__linux__)
            const Result wlCopy = copyWithProcess("wl-copy", {}, text);
            if (wlCopy || wlCopy.message.find("No clipboard backend") == std::string::npos)
                return wlCopy;
            const Result xclip = copyWithProcess("xclip", {"-selection", "clipboard"}, text);
            if (xclip || xclip.message.find("No clipboard backend") == std::string::npos)
                return xclip;
            const Result xsel = copyWithProcess("xsel", {"--clipboard", "--input"}, text);
            if (xsel || xsel.message.find("No clipboard backend") == std::string::npos)
                return xsel;
            return Result::Failure("No clipboard backend is available (tried wl-copy, xclip and xsel)",
                ResultCode::Unavailable, true);
#else
            return Result::Failure("Clipboard support is not available on this platform", ResultCode::Unavailable);
#endif
        }

    }
}

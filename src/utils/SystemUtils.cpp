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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
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
                pid_t pid = fork();
                if (pid == 0) {
#ifdef __APPLE__
                    const char* cmd = "open";
#else
                    const char* cmd = "xdg-open";
#endif
                    execlp(cmd, cmd, path.c_str(), (char*)NULL);
                    _exit(127);
                }
                if (pid < 0)
                    return Result::Failure("Unable to launch the operating system opener", ResultCode::Unavailable, true);
                int status = 0;
                if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                    return Result::Failure("The operating system opener failed", ResultCode::Unavailable, true);
                return Result::Success();
#else
                return Result::Failure("Opening paths is not supported on this platform", ResultCode::Unavailable);
#endif
            }
        }

        SystemOpener::SystemOpener(std::size_t pendingLimit)
            : maxPending(std::max<std::size_t>(1, pendingLimit)), worker(&SystemOpener::workerLoop, this) {}

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
            Result result = launchPlatformProcess(request.kind, request.path);
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

    }
}

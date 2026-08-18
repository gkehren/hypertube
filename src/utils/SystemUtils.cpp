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
#include <charconv>
#include <optional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>
#include <cerrno>
#include <cstring>
extern char **environ;
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
            bool equalsIgnoreCase(const std::string &left, const char *right) {
                if (right == nullptr)
                    return false;
                const std::string expected(right);
                if (left.size() != expected.size())
                    return false;
                for (std::size_t index = 0; index < left.size(); ++index) {
                    if (std::tolower(static_cast<unsigned char>(left[index]))
                        != std::tolower(static_cast<unsigned char>(expected[index])))
                        return false;
                }
                return true;
            }

            bool containsIgnoreCase(const std::string &value, const char *needle) {
                std::string lowered = value;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                    [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                std::string expected = needle ? needle : "";
                std::transform(expected.begin(), expected.end(), expected.begin(),
                    [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                return !expected.empty() && lowered.find(expected) != std::string::npos;
            }

            bool parseColorFgbgBackground(const std::string &value, bool &dark) {
                const std::size_t separator = value.find_last_of(';');
                if (separator == std::string::npos || separator + 1 >= value.size())
                    return false;
                const std::string background = value.substr(separator + 1);
                int index = 0;
                const auto parsed = std::from_chars(background.data(), background.data() + background.size(), index);
                if (parsed.ec != std::errc{} || parsed.ptr != background.data() + background.size())
                    return false;
                dark = index < 8;
                return true;
            }

            SystemAppearance parseAppearanceText(const std::string &value) {
                if (containsIgnoreCase(value, "dark") || containsIgnoreCase(value, "prefer-dark"))
                    return SystemAppearance::Dark;
                if (containsIgnoreCase(value, "light") || containsIgnoreCase(value, "prefer-light"))
                    return SystemAppearance::Light;
                return SystemAppearance::Unavailable;
            }

#ifndef _WIN32
            std::optional<std::string> readProcessOutput(const char *program,
                std::initializer_list<const char *> arguments) {
                int pipes[2] = {-1, -1};
                if (pipe(pipes) != 0)
                    return std::nullopt;
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
                    return std::nullopt;
                }
                posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
                posix_spawn_file_actions_addclose(&actions, pipes[0]);
                posix_spawn_file_actions_addclose(&actions, pipes[1]);
                pid_t pid = -1;
                const int error = posix_spawnp(&pid, program, &actions, nullptr, argv.data(), environ);
                posix_spawn_file_actions_destroy(&actions);
                close(pipes[1]);
                if (error != 0) {
                    close(pipes[0]);
                    return std::nullopt;
                }
                std::string output;
                char buffer[512];
                for (;;) {
                    const ssize_t count = read(pipes[0], buffer, sizeof(buffer));
                    if (count <= 0)
                        break;
                    output.append(buffer, static_cast<std::size_t>(count));
                    if (output.size() > 8192) {
                        output.resize(8192);
                        break;
                    }
                }
                close(pipes[0]);
                int status = 0;
                if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                    return std::nullopt;
                return output;
            }
#endif

            SystemAppearance nativeSystemAppearance() {
#ifdef _WIN32
                HKEY key = nullptr;
                if (RegOpenKeyExA(HKEY_CURRENT_USER,
                        "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        0, KEY_READ, &key) != ERROR_SUCCESS)
                    return SystemAppearance::Unavailable;
                DWORD value = 1;
                DWORD size = sizeof(value);
                const LONG result = RegQueryValueExA(key, "AppsUseLightTheme", nullptr, nullptr,
                    reinterpret_cast<LPBYTE>(&value), &size);
                RegCloseKey(key);
                if (result != ERROR_SUCCESS || size != sizeof(value))
                    return SystemAppearance::Unavailable;
                return value == 0 ? SystemAppearance::Dark : SystemAppearance::Light;
#elif defined(__APPLE__)
                const auto *value = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"),
                    kCFPreferencesAnyApplication);
                if (value == nullptr)
                    return SystemAppearance::Light;
                SystemAppearance appearance = SystemAppearance::Unavailable;
                if (CFGetTypeID(value) == CFStringGetTypeID()) {
                    char buffer[64] = {};
                    if (CFStringGetCString(static_cast<CFStringRef>(value), buffer, sizeof(buffer),
                            kCFStringEncodingUTF8))
                        appearance = parseAppearanceText(buffer);
                }
                CFRelease(value);
                return appearance;
#elif defined(__linux__)
                if (const auto colorScheme = readProcessOutput("gsettings",
                        {"get", "org.gnome.desktop.interface", "color-scheme"})) {
                    const auto appearance = parseAppearanceText(*colorScheme);
                    if (appearance != SystemAppearance::Unavailable)
                        return appearance;
                }
                if (const auto gtkTheme = readProcessOutput("gsettings",
                        {"get", "org.gnome.desktop.interface", "gtk-theme"})) {
                    const auto appearance = parseAppearanceText(*gtkTheme);
                    if (appearance != SystemAppearance::Unavailable)
                        return appearance;
                }
                if (const auto kdeTheme = readProcessOutput("kreadconfig6",
                        {"--group", "General", "--key", "ColorScheme"})) {
                    const auto appearance = parseAppearanceText(*kdeTheme);
                    if (appearance != SystemAppearance::Unavailable)
                        return appearance;
                }
                if (const auto kdeTheme = readProcessOutput("kreadconfig5",
                        {"--group", "General", "--key", "ColorScheme"})) {
                    const auto appearance = parseAppearanceText(*kdeTheme);
                    if (appearance != SystemAppearance::Unavailable)
                        return appearance;
                }
#endif
                return SystemAppearance::Unavailable;
            }

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

        SystemAppearance systemAppearance() {
            if (const char *overrideTheme = std::getenv("HYPERTUBE_SYSTEM_THEME"); overrideTheme) {
                const std::string value(overrideTheme);
                if (equalsIgnoreCase(value, "light"))
                    return SystemAppearance::Light;
                if (equalsIgnoreCase(value, "dark"))
                    return SystemAppearance::Dark;
            }

            const auto native = nativeSystemAppearance();
            if (native != SystemAppearance::Unavailable)
                return native;

            if (const char *gtkTheme = std::getenv("GTK_THEME"); gtkTheme) {
                const std::string value(gtkTheme);
                if (containsIgnoreCase(value, "dark"))
                    return SystemAppearance::Dark;
                if (containsIgnoreCase(value, "light"))
                    return SystemAppearance::Light;
            }

            if (const char *colorFgbg = std::getenv("COLORFGBG"); colorFgbg) {
                bool dark = true;
                if (parseColorFgbgBackground(colorFgbg, dark))
                    return dark ? SystemAppearance::Dark : SystemAppearance::Light;
            }

            return SystemAppearance::Unavailable;
        }

        bool systemPrefersDarkTheme() {
            return systemAppearance() != SystemAppearance::Light;
        }

    }
}

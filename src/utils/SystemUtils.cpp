#include "SystemUtils.hpp"
#include <cstdlib>
#include <thread>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <cctype>

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

        std::string sanitizePath(const std::string& path) {
            if (!path.empty() && path[0] == '-') {
#ifdef _WIN32
                return ".\\" + path;
#else
                return "./" + path;
#endif
            }
            return path;
        }

        void openFileExplorer(const std::string& path) {
#ifdef _WIN32
            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([path]() {
                std::string safePath = sanitizePath(path);
                // ShellExecute is safer than system() as it doesn't involve a shell for parsing
                HINSTANCE result = ShellExecuteA(NULL, "open", safePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32) {
                    std::cerr << "Failed to open file explorer for path: " << path << std::endl;
                }
            }).detach();
#elif defined(__APPLE__) || defined(__linux__)
            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([path]() {
                pid_t pid = fork();
                if (pid == 0) {
                    std::string safePath = sanitizePath(path);
                    // Child process: execute the command directly without a shell
#ifdef __APPLE__
                    const char* cmd = "open";
#else
                    const char* cmd = "xdg-open";
#endif
                    execlp(cmd, cmd, safePath.c_str(), (char*)NULL);
                    // If execlp fails, exit the child process
                    _exit(1);
                } else if (pid > 0) {
                    // Parent process (in thread): wait for the child to prevent zombies
                    int status;
                    waitpid(pid, &status, 0);
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        std::cerr << "Failed to open file explorer for path: " << path << std::endl;
                    }
                } else {
                    std::cerr << "Failed to fork process for path: " << path << std::endl;
                }
            }).detach();
#else
            std::cerr << "Unsupported platform for openFileExplorer" << std::endl;
#endif
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

        void openFilePreview(const std::string& filePath) {
#ifdef _WIN32
            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([filePath]() {
                std::string safePath = sanitizePath(filePath);
                // ShellExecute is safer than system() as it doesn't involve a shell for parsing
                HINSTANCE result = ShellExecuteA(NULL, "open", safePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32) {
                    std::cerr << "Failed to open file for preview: " << filePath << std::endl;
                }
            }).detach();
#elif defined(__APPLE__) || defined(__linux__)
            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([filePath]() {
                pid_t pid = fork();
                if (pid == 0) {
                    std::string safePath = sanitizePath(filePath);
                    // Child process: execute the command directly without a shell
#ifdef __APPLE__
                    const char* cmd = "open";
#else
                    const char* cmd = "xdg-open";
#endif
                    execlp(cmd, cmd, safePath.c_str(), (char*)NULL);
                    // If execlp fails, exit the child process
                    _exit(1);
                } else if (pid > 0) {
                    // Parent process (in thread): wait for the child to prevent zombies
                    int status;
                    waitpid(pid, &status, 0);
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        std::cerr << "Failed to open file for preview: " << filePath << std::endl;
                    }
                } else {
                    std::cerr << "Failed to fork process for file: " << filePath << std::endl;
                }
            }).detach();
#else
            std::cerr << "Unsupported platform for openFilePreview" << std::endl;
#endif
        }

    }
}

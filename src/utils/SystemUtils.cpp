#include "SystemUtils.hpp"
#include <cstdlib>
#include <thread>
#include <iostream>
#include <ctime>

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

        void openFileExplorer(const std::string& path) {
#ifdef _WIN32
            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([path]() {
                // ShellExecute is safer than system() as it doesn't involve a shell for parsing
                HINSTANCE result = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32) {
                    std::cerr << "Failed to open file explorer for path: " << path << std::endl;
                }
            }).detach();
#elif defined(__APPLE__) || defined(__linux__)
            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([path]() {
                pid_t pid = fork();
                if (pid == 0) {
                    // Child process: execute the command directly without a shell
#ifdef __APPLE__
                    const char* cmd = "open";
#else
                    const char* cmd = "xdg-open";
#endif
                    execlp(cmd, cmd, path.c_str(), (char*)NULL);
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

    }
}

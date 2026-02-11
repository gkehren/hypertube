#include "SystemUtils.hpp"
#include <cstdlib>
#include <thread>
#include <iostream>
#include <ctime>

namespace Utils {
    namespace SystemUtils {

        void openFileExplorer(const std::string& path) {
            std::string command;

#ifdef _WIN32
            command = "explorer.exe \"" + path + "\"";
#elif __APPLE__
            command = "open \"" + path + "\"";
#elif __linux__
            command = "xdg-open \"" + path + "\"";
#else
            std::cerr << "Unsupported platform for openFileExplorer" << std::endl;
            return;
#endif

            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([command]() {
                int ret = std::system(command.c_str());
                if (ret != 0) {
                    std::cerr << "Failed to open file explorer with command: " << command << std::endl;
                }
            }).detach();
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

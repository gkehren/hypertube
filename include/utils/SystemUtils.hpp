#pragma once

#include <string>
#include <ctime>

namespace Utils {
    namespace SystemUtils {
        /**
         * @brief Opens the file explorer at the specified path asynchronously.
         *
         * This function launches the platform-specific file explorer (explorer.exe, open, xdg-open)
         * in a detached thread to prevent blocking the calling thread.
         *
         * @param path The path to open. Can be a directory or a file.
         */
        void openFileExplorer(const std::string& path);

        /**
         * @brief Thread-safe wrapper for std::localtime.
         *
         * Uses localtime_r on POSIX and localtime_s on Windows.
         *
         * @param time The time_t value to convert.
         * @param result The tm structure to fill with the local time.
         * @return true if successful, false otherwise.
         */
        bool getLocalTime(const std::time_t& time, std::tm& result);
    }
}

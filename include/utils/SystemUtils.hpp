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

        /**
         * @brief Determines if a file is previewable based on its extension.
         *
         * Checks if the file is a video, audio, image, or text file that can be previewed.
         *
         * @param filename The name of the file to check.
         * @return true if the file can be previewed, false otherwise.
         */
        bool isPreviewableFile(const std::string& filename);

        /**
         * @brief Opens a file with the system's default application for preview.
         *
         * This function launches the platform-specific default application (e.g., video player,
         * image viewer, text editor) in a detached thread to prevent blocking the calling thread.
         *
         * @param filePath The full path to the file to preview.
         */
        void openFilePreview(const std::string& filePath);

        /**
         * @brief Sanitizes a path to prevent it from being interpreted as a command-line argument.
         *
         * @param path The path to sanitize.
         * @return The sanitized path.
         */
        std::string sanitizePath(const std::string& path);
    }
}

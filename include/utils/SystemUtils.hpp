#pragma once

#include <string>

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
    }
}

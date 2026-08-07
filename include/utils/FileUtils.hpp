#pragma once

#include <filesystem>
#include <string>

namespace Utils {
    namespace FileUtils {
        /**
         * @brief Atomically and durably writes content to a file.
         * 
         * Writes content to a unique temporary file on the same filesystem/directory,
         * flushes OS buffers to physical disk (fsync/FlushFileBuffers), maintains a .bak candidate
         * if the destination file already exists, and atomically replaces the target file.
         * 
         * @param target The final destination path.
         * @param content The text/binary data to write.
         * @param errorMessage Output error message if the operation fails.
         * @return true if successful, false otherwise.
         */
        bool durableWriteFile(const std::filesystem::path &target, const std::string &content, std::string &errorMessage);
    }
}

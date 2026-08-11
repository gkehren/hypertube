#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace Utils {
    namespace FileUtils {
        struct FileOperations {
            virtual ~FileOperations() = default;
            virtual bool createDirectories(const std::filesystem::path &parent, std::error_code &ec);
            virtual bool openAndWriteTemp(const std::filesystem::path &tempPath, const std::string &content, std::string &errorMessage);
            virtual bool flushToDisk(const std::filesystem::path &filePath);
            virtual bool copyFile(const std::filesystem::path &from, const std::filesystem::path &to, std::filesystem::copy_options options, std::error_code &ec);
            virtual bool replaceOrRenameFile(const std::filesystem::path &from, const std::filesystem::path &to, std::string &errorMessage);
            virtual bool syncParentDirectory(const std::filesystem::path &dirPath);
            virtual bool exists(const std::filesystem::path &p, std::error_code &ec);
            virtual bool remove(const std::filesystem::path &p, std::error_code &ec);
        };

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
         * @param ops Injectable FileOperations abstraction for failure testing (nullptr uses default).
         * @return true if successful, false otherwise.
         */
        bool durableWriteFile(const std::filesystem::path &target, const std::string &content, std::string &errorMessage, FileOperations *ops = nullptr);
    }
}

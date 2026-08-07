#include "FileUtils.hpp"
#include <fstream>
#include <system_error>
#include <random>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace Utils {
    namespace FileUtils {
        namespace {
            std::atomic<std::uint64_t> tempCounter{0};

            std::filesystem::path generateTempPath(const std::filesystem::path &target) {
                const auto parent = target.parent_path();
                const auto filename = target.filename().string();
                const auto uniqueId = std::to_string(
#ifdef _WIN32
                    GetCurrentProcessId()
#else
                    getpid()
#endif
                ) + "_" + std::to_string(++tempCounter);
                return parent / (filename + "." + uniqueId + ".tmp");
            }

            bool flushToDisk(const std::filesystem::path &filePath) {
#ifdef _WIN32
                HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE)
                    return false;
                BOOL success = FlushFileBuffers(hFile);
                CloseHandle(hFile);
                return success != FALSE;
#else
                int fd = open(filePath.c_str(), O_WRONLY);
                if (fd < 0)
                    return false;
#if defined(__APPLE__)
                int res = fcntl(fd, F_FULLFSYNC);
#else
                int res = fdatasync(fd);
#endif
                close(fd);
                return res == 0;
#endif
            }

            void syncParentDirectory(const std::filesystem::path &dirPath) {
#ifndef _WIN32
                int dirfd = open(dirPath.c_str(), O_RDONLY);
                if (dirfd >= 0) {
                    fsync(dirfd);
                    close(dirfd);
                }
#endif
            }
        }

        bool durableWriteFile(const std::filesystem::path &target, const std::string &content, std::string &errorMessage) {
            std::error_code error;
            const auto parent = target.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent, error);
                if (error) {
                    errorMessage = "Unable to create target directory: " + error.message();
                    return false;
                }
            }

            const auto temporary = generateTempPath(target);
            {
                std::ofstream file(temporary, std::ios::out | std::ios::binary | std::ios::trunc);
                if (!file.is_open()) {
                    errorMessage = "Unable to open temporary file: " + temporary.string();
                    return false;
                }
                file.write(content.data(), static_cast<std::streamsize>(content.size()));
                file.flush();
                if (!file.good()) {
                    errorMessage = "Failed to write content to temporary file";
                    std::filesystem::remove(temporary, error);
                    return false;
                }
            }

            if (!flushToDisk(temporary)) {
                errorMessage = "Failed to flush file buffers to disk";
                std::filesystem::remove(temporary, error);
                return false;
            }

            if (std::filesystem::exists(target, error)) {
                const auto backup = target.string() + ".bak";
                std::filesystem::copy_file(target, backup, std::filesystem::copy_options::overwrite_existing, error);
                if (error) {
                    errorMessage = "Unable to create configuration backup: " + error.message();
                    std::filesystem::remove(temporary, error);
                    return false;
                }
            }

#ifdef _WIN32
            if (std::filesystem::exists(target, error)) {
                BOOL replaced = ReplaceFileW(target.c_str(), temporary.c_str(), NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
                if (!replaced) {
                    BOOL moved = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
                    if (!moved) {
                        errorMessage = "Unable to replace file on Windows";
                        std::filesystem::remove(temporary, error);
                        return false;
                    }
                }
            } else {
                BOOL moved = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
                if (!moved) {
                    errorMessage = "Unable to create destination file on Windows";
                    std::filesystem::remove(temporary, error);
                    return false;
                }
            }
#else
            std::filesystem::rename(temporary, target, error);
            if (error) {
                errorMessage = "Unable to replace file: " + error.message();
                std::filesystem::remove(temporary, error);
                return false;
            }
            syncParentDirectory(parent);
#endif

            return true;
        }
    }
}

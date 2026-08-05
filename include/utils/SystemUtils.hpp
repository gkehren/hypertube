#pragma once

#include <string>
#include <ctime>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include "Result.hpp"

namespace Utils {
    namespace SystemUtils {
        enum class OpenOperationKind { Explorer, Preview };

        struct OpenOperationResult {
            std::uint64_t id = 0;
            OpenOperationKind kind = OpenOperationKind::Explorer;
            Result result = Result::Success();
        };

        class SystemOpener {
        public:
            explicit SystemOpener(std::size_t maxPending = 32);
            ~SystemOpener();
            SystemOpener(const SystemOpener &) = delete;
            SystemOpener &operator=(const SystemOpener &) = delete;

            Result enqueueExplorer(const std::string &path, std::uint64_t *id = nullptr);
            Result enqueuePreview(const std::string &path, std::uint64_t *id = nullptr);
            std::vector<OpenOperationResult> drainResults();

        private:
            struct Request {
                std::uint64_t id;
                OpenOperationKind kind;
                std::string path;
            };
            std::size_t maxPending;
            std::mutex mutex;
            std::condition_variable condition;
            std::deque<Request> requests;
            std::deque<OpenOperationResult> results;
            std::thread worker;
            bool stopping = false;
            std::uint64_t nextId = 1;

            Result enqueue(OpenOperationKind kind, const std::string &path, std::uint64_t *id);
            Result execute(const Request &request);
            void workerLoop();
        };

        /**
         * @brief Opens the file explorer at the specified path immediately.
         *
         * Legacy immediate validation helper. New UI code should use SystemOpener.
         *
         * @param path The path to open. Can be a directory or a file.
         */
        Result openFileExplorer(const std::string& path);

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
         * This legacy helper launches the platform-specific default application immediately.
         * New UI code should use SystemOpener to keep process execution off the render thread.
         *
         * @param filePath The full path to the file to preview.
         */
        Result openFilePreview(const std::string& filePath);
    }
}

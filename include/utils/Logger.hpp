#pragma once

#include <cstddef>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <cstdint>
#include <vector>

namespace Utils
{
enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error
};

struct LogRecord
{
	std::chrono::system_clock::time_point timestamp;
	LogLevel level;
	std::string category;
	std::string message;
};

class Logger
{
public:
	static void initialize(const std::filesystem::path &path);
	static void log(LogLevel level, const std::string &category, const std::string &message);
	static void debug(const std::string &category, const std::string &message);
	static void info(const std::string &category, const std::string &message);
	static void warning(const std::string &category, const std::string &message);
	static void error(const std::string &category, const std::string &message);
	static std::vector<LogRecord> recent();
	static std::uint64_t revision();
	static void clearRecent();
	static std::filesystem::path logPath();

	// Log rotation configuration
	static void setMaxLogSize(std::size_t maxBytes);
	static void setMaxBackupFiles(std::size_t count);

	// Diagnostics export
	static std::string formatDiagnostics();
	static bool exportDiagnosticsToFile(const std::filesystem::path &targetPath, std::string &errorMessage);

private:
	static std::mutex &mutex();
	static void rotateLogsIfNeededUnlocked();
};
} // namespace Utils

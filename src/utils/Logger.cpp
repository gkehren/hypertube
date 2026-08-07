#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdint>

#include "AppPaths.hpp"
#include "FileUtils.hpp"

namespace
{
std::filesystem::path g_logPath;
std::deque<Utils::LogRecord> g_recent;
std::uint64_t g_revision = 0;
constexpr std::size_t MAX_RECENT_LOGS = 1000;
std::size_t g_maxLogSizeBytes = 5 * 1024 * 1024; // Default 5MB
std::size_t g_maxBackupFiles = 3;

const char *levelName(Utils::LogLevel level)
{
	switch (level)
	{
	case Utils::LogLevel::Debug:
		return "DEBUG";
	case Utils::LogLevel::Info:
		return "INFO";
	case Utils::LogLevel::Warning:
		return "WARN";
	case Utils::LogLevel::Error:
		return "ERROR";
	}
	return "INFO";
}

std::string timestamp()
{
	const auto now = std::chrono::system_clock::now();
	const auto time = std::chrono::system_clock::to_time_t(now);
	std::tm localTime{};
#ifdef _WIN32
	localtime_s(&localTime, &time);
#else
	localtime_r(&time, &localTime);
#endif
	std::ostringstream output;
	output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
	return output.str();
}
} // namespace

namespace Utils
{
std::mutex &Logger::mutex()
{
	static std::mutex value;
	return value;
}

void Logger::initialize(const std::filesystem::path &path)
{
	std::lock_guard<std::mutex> lock(mutex());
	g_logPath = path;
	std::error_code error;
	if (!g_logPath.parent_path().empty())
		std::filesystem::create_directories(g_logPath.parent_path(), error);
}

void Logger::setMaxLogSize(std::size_t maxBytes)
{
	std::lock_guard<std::mutex> lock(mutex());
	g_maxLogSizeBytes = maxBytes > 0 ? maxBytes : 5 * 1024 * 1024;
}

void Logger::setMaxBackupFiles(std::size_t count)
{
	std::lock_guard<std::mutex> lock(mutex());
	g_maxBackupFiles = count;
}

void Logger::rotateLogsIfNeededUnlocked()
{
	std::error_code error;
	if (!std::filesystem::exists(g_logPath, error))
		return;

	const auto size = std::filesystem::file_size(g_logPath, error);
	if (error || size < g_maxLogSizeBytes)
		return;

	for (std::size_t i = g_maxBackupFiles; i > 1; --i)
	{
		auto oldFile = g_logPath.string() + "." + std::to_string(i - 1);
		auto newFile = g_logPath.string() + "." + std::to_string(i);
		if (std::filesystem::exists(oldFile, error))
		{
			std::filesystem::rename(oldFile, newFile, error);
		}
	}

	if (g_maxBackupFiles > 0)
	{
		auto firstBackup = g_logPath.string() + ".1";
		std::filesystem::rename(g_logPath, firstBackup, error);
	}
	else
	{
		std::filesystem::remove(g_logPath, error);
	}
}

void Logger::log(LogLevel level, const std::string &category, const std::string &message)
{
	std::lock_guard<std::mutex> lock(mutex());
	if (g_logPath.empty())
		g_logPath = AppPaths::logFilePath();

	LogRecord record{std::chrono::system_clock::now(), level, category, message};
	g_recent.push_back(record);
	++g_revision;
	while (g_recent.size() > MAX_RECENT_LOGS)
		g_recent.pop_front();

	rotateLogsIfNeededUnlocked();

	std::error_code error;
	if (!g_logPath.parent_path().empty())
		std::filesystem::create_directories(g_logPath.parent_path(), error);
	std::ofstream file(g_logPath, std::ios::app);
	if (file.is_open())
		file << timestamp() << " [" << levelName(level) << "] [" << category << "] " << message << '\n';
}

void Logger::debug(const std::string &category, const std::string &message)
{
	log(LogLevel::Debug, category, message);
}

void Logger::info(const std::string &category, const std::string &message)
{
	log(LogLevel::Info, category, message);
}

void Logger::warning(const std::string &category, const std::string &message)
{
	log(LogLevel::Warning, category, message);
}

void Logger::error(const std::string &category, const std::string &message)
{
	log(LogLevel::Error, category, message);
}

std::vector<LogRecord> Logger::recent()
{
	std::lock_guard<std::mutex> lock(mutex());
	return std::vector<LogRecord>(g_recent.begin(), g_recent.end());
}

std::uint64_t Logger::revision()
{
	std::lock_guard<std::mutex> lock(mutex());
	return g_revision;
}

void Logger::clearRecent()
{
	std::lock_guard<std::mutex> lock(mutex());
	g_recent.clear();
	++g_revision;
}

std::filesystem::path Logger::logPath()
{
	std::lock_guard<std::mutex> lock(mutex());
	if (g_logPath.empty())
		g_logPath = AppPaths::logFilePath();
	return g_logPath;
}

std::string Logger::formatDiagnostics()
{
	std::lock_guard<std::mutex> lock(mutex());
	std::ostringstream ss;
	ss << "========================================\n";
	ss << "Hypertube Application Diagnostics Report\n";
	ss << "========================================\n";
	ss << "Report Timestamp: " << timestamp() << "\n";
	ss << "Portable Mode: " << (AppPaths::isPortable() ? "Yes" : "No") << "\n";
	ss << "Config Dir: " << AppPaths::configDirectory().string() << "\n";
	ss << "Data Dir: " << AppPaths::dataDirectory().string() << "\n";
	ss << "Log Path: " << (g_logPath.empty() ? AppPaths::logFilePath() : g_logPath).string() << "\n";
	ss << "Log Rotation Size Limit: " << (g_maxLogSizeBytes / 1024 / 1024) << " MB, Max Backups: " << g_maxBackupFiles << "\n";
	ss << "Recent Buffer Entry Count: " << g_recent.size() << "\n\n";

	ss << "--- Recent Log Entries (" << g_recent.size() << ") ---\n";
	for (const auto &rec : g_recent)
	{
		const auto time = std::chrono::system_clock::to_time_t(rec.timestamp);
		std::tm localTime{};
#ifdef _WIN32
		localtime_s(&localTime, &time);
#else
		localtime_r(&time, &localTime);
#endif
		ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " [" << levelName(rec.level) << "] [" << rec.category << "] " << rec.message << "\n";
	}
	return ss.str();
}

bool Logger::exportDiagnosticsToFile(const std::filesystem::path &targetPath, std::string &errorMessage)
{
	const std::string content = formatDiagnostics();
	return FileUtils::durableWriteFile(targetPath, content, errorMessage);
}

} // namespace Utils

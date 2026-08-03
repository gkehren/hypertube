#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "AppPaths.hpp"

namespace
{
std::filesystem::path g_logPath;
std::deque<Utils::LogRecord> g_recent;
constexpr std::size_t MAX_RECENT_LOGS = 1000;

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

void Logger::log(LogLevel level, const std::string &category, const std::string &message)
{
	std::lock_guard<std::mutex> lock(mutex());
	if (g_logPath.empty())
		g_logPath = AppPaths::logFilePath();

	LogRecord record{std::chrono::system_clock::now(), level, category, message};
	g_recent.push_back(record);
	while (g_recent.size() > MAX_RECENT_LOGS)
		g_recent.pop_front();

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

void Logger::clearRecent()
{
	std::lock_guard<std::mutex> lock(mutex());
	g_recent.clear();
}

std::filesystem::path Logger::logPath()
{
	std::lock_guard<std::mutex> lock(mutex());
	if (g_logPath.empty())
		g_logPath = AppPaths::logFilePath();
	return g_logPath;
}
} // namespace Utils

#include "Logger.hpp"

#include <chrono>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <vector>

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
std::unique_ptr<std::ofstream> g_logStream;

void closeLogStreamUnlocked()
{
	if (g_logStream && g_logStream->is_open())
	{
		g_logStream->close();
	}
	g_logStream.reset();
}

void ensureLogStreamUnlocked()
{
	if ((!g_logStream || !g_logStream->is_open()) && !g_logPath.empty())
	{
		std::error_code error;
		if (!g_logPath.parent_path().empty())
			std::filesystem::create_directories(g_logPath.parent_path(), error);
		if (!g_logStream)
			g_logStream = std::make_unique<std::ofstream>();
		g_logStream->open(g_logPath, std::ios::app);
	}
}

std::string redactSensitiveData(const std::string &input)
{
	std::string text = input;
	auto lowercase = [](const std::string &value) {
		std::string result = value;
		std::transform(result.begin(), result.end(), result.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return result;
	};

	// 1. Redact URL credentials (e.g., http://user:pass@host)
	std::size_t schemePos = 0;
	while ((schemePos = text.find("://", schemePos)) != std::string::npos)
	{
		std::size_t userStart = schemePos + 3;
		std::size_t atPos = text.find('@', userStart);
		std::size_t slashPos = text.find_first_of("/? \r\n", userStart);
		if (atPos != std::string::npos && (slashPos == std::string::npos || atPos < slashPos))
		{
			std::size_t colonPos = text.find(':', userStart);
			if (colonPos != std::string::npos && colonPos < atPos)
			{
				text.replace(colonPos + 1, atPos - (colonPos + 1), "[REDACTED]");
				schemePos = atPos + 10;
				continue;
			}
		}
		schemePos += 3;
	}

	// 2. Redact authorization headers and bearer credentials. Preserve an
	// Authorization scheme such as Bearer or Basic, but never its value.
	static const std::vector<std::pair<std::string, bool>> authPrefixes = {
		{ "bearer ", false },
		{ "authorization:", true },
		{ "authorization=", true },
		{ "x-api-key:", false },
		{ "x-api-key=", false }
	};
	for (const auto &[prefix, preserveScheme] : authPrefixes)
	{
		std::string lowerText = lowercase(text);
		std::size_t pos = 0;
		while ((pos = lowerText.find(prefix, pos)) != std::string::npos)
		{
			std::size_t valStart = pos + prefix.length();
			while (valStart < text.length() && (text[valStart] == ' ' || text[valStart] == '\t'))
				++valStart;
			if (preserveScheme)
			{
				const std::size_t schemeEnd = text.find_first_of(" \t", valStart);
				if (schemeEnd != std::string::npos)
				{
					valStart = schemeEnd;
					while (valStart < text.length() && (text[valStart] == ' ' || text[valStart] == '\t'))
						++valStart;
				}
			}
			std::size_t valEnd = text.find_first_of("&, \t\r\n;\"", valStart);
			if (valEnd == std::string::npos)
				valEnd = text.length();
			if (valEnd > valStart)
			{
				text.replace(valStart, valEnd - valStart, "[REDACTED]");
			}
			pos = valStart + 10;
			lowerText = lowercase(text);
		}
	}

	// 3. Case-insensitive key=value and key: value pattern matching
	static const std::vector<std::string> keys = {
		"password=", "password:", "api_key=", "api_key:", "secret=", "secret:",
		"token=", "token:", "apikey=", "apikey:", "pass=", "pass:",
		"access_token=", "access_token:"
	};

	std::string lowerText = lowercase(text);

	for (const auto &key : keys)
	{
		std::size_t pos = 0;
		while ((pos = lowerText.find(key, pos)) != std::string::npos)
		{
			std::size_t valStart = pos + key.length();
			while (valStart < text.length() && (text[valStart] == ' ' || text[valStart] == '\t'))
				++valStart;
			std::size_t valEnd = text.find_first_of("&, \t\r\n;\"", valStart);
			if (valEnd == std::string::npos)
				valEnd = text.length();
			if (valEnd > valStart)
			{
				text.replace(valStart, valEnd - valStart, "[REDACTED]");
			}
			pos = valStart + 10;
			lowerText = lowercase(text);
		}
	}

	return text;
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
	closeLogStreamUnlocked();
	g_logPath = path;
	if (!g_logPath.empty())
		ensureLogStreamUnlocked();
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
	if (g_logStream && g_logStream->is_open())
	{
		g_logStream->flush();
		const auto pos = g_logStream->tellp();
		if (pos != -1 && static_cast<std::size_t>(pos) < g_maxLogSizeBytes)
			return;
		closeLogStreamUnlocked();
	}

	std::error_code error;
	if (!std::filesystem::exists(g_logPath, error))
		return;

	const auto size = std::filesystem::file_size(g_logPath, error);
	if (error || size < g_maxLogSizeBytes)
		return;

	closeLogStreamUnlocked();

	for (std::size_t i = g_maxBackupFiles; i > 1; --i)
	{
		auto oldFile = g_logPath.string() + "." + std::to_string(i - 1);
		auto newFile = g_logPath.string() + "." + std::to_string(i);
		if (std::filesystem::exists(oldFile, error))
		{
			std::filesystem::remove(newFile, error);
			std::filesystem::rename(oldFile, newFile, error);
		}
	}

	if (g_maxBackupFiles > 0)
	{
		auto firstBackup = g_logPath.string() + ".1";
		std::filesystem::remove(firstBackup, error);
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

	ensureLogStreamUnlocked();
	if (g_logStream && g_logStream->is_open())
	{
		*g_logStream << timestamp() << " [" << levelName(level) << "] [" << category << "] " << message << '\n';
		g_logStream->flush();
	}
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
		ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " [" << levelName(rec.level) << "] [" << rec.category << "] " << redactSensitiveData(rec.message) << "\n";
	}
	return ss.str();
}

bool Logger::exportDiagnosticsToFile(const std::filesystem::path &targetPath, std::string &errorMessage)
{
	const std::string content = formatDiagnostics();
	return FileUtils::durableWriteFile(targetPath, content, errorMessage);
}

} // namespace Utils

#include "presentation/LogsPresenter.hpp"

#include "TorrentManager.hpp"
#include "presentation/UiFormatters.hpp"

#include <sstream>

namespace
{
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

int severity(Utils::LogLevel level)
{
	return static_cast<int>(level);
}
} // namespace

namespace Presentation
{
LogsPresenter::LogsPresenter(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void LogsPresenter::update()
{
	for (const auto &event : torrentManager.drainEvents())
	{
		if (!event.message.empty())
			addLogEntry(event.category, event.message, event.severity);
	}
}

void LogsPresenter::clear()
{
	Utils::Logger::clearRecent();
}

void LogsPresenter::setLevelEnabled(Utils::LogLevel level, bool enabled)
{
	switch (level)
	{
	case Utils::LogLevel::Debug: showDebug_ = enabled; break;
	case Utils::LogLevel::Info: showInfo_ = enabled; break;
	case Utils::LogLevel::Warning: showWarnings_ = enabled; break;
	case Utils::LogLevel::Error: showErrors_ = enabled; break;
	}
}

bool LogsPresenter::levelEnabled(Utils::LogLevel level) const
{
	switch (level)
	{
	case Utils::LogLevel::Debug: return showDebug_;
	case Utils::LogLevel::Info: return showInfo_;
	case Utils::LogLevel::Warning: return showWarnings_;
	case Utils::LogLevel::Error: return showErrors_;
	}
	return false;
}

std::vector<LogRowDto> LogsPresenter::buildRows() const
{
	const auto diagnostics = Utils::Logger::recent();
	const std::size_t first = diagnostics.size() > maxEntries_ ? diagnostics.size() - maxEntries_ : 0;
	std::vector<LogRowDto> rows;
	rows.reserve(diagnostics.size() - first);
	for (std::size_t index = first; index < diagnostics.size(); ++index)
	{
		const auto &record = diagnostics[index];
		if (!levelEnabled(record.level))
			continue;
		rows.push_back({
			UiFormatters::formatTimestamp(record.timestamp),
			levelName(record.level),
			record.category,
			record.message,
			severity(record.level)});
	}
	return rows;
}

void LogsPresenter::addLogEntry(const std::string &category, const std::string &message, Utils::LogLevel level)
{
	Utils::Logger::log(level, category, message);
}
} // namespace Presentation

#include "presentation/LogsPresenter.hpp"

#include "TorrentManager.hpp"
#include "presentation/UiFormatters.hpp"

#include <libtorrent/alert_types.hpp>

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

void LogsPresenter::processAlert(lt::alert *alert)
{
	if (!alert)
		return;

	if (auto *trackerError = lt::alert_cast<lt::tracker_error_alert>(alert))
	{
		addLogEntry("tracker", std::string("Tracker error for '") + trackerError->torrent_name() + "': " + trackerError->error_message(), Utils::LogLevel::Error);
	}
	else if (auto *trackerWarning = lt::alert_cast<lt::tracker_warning_alert>(alert))
	{
		addLogEntry("tracker", std::string("Tracker warning for '") + trackerWarning->torrent_name() + "': " + trackerWarning->warning_message(), Utils::LogLevel::Warning);
	}
	else if (auto *fileError = lt::alert_cast<lt::file_error_alert>(alert))
	{
		addLogEntry("storage", std::string("File error for '") + fileError->torrent_name() + "': " + fileError->error.message(), Utils::LogLevel::Error);
	}
	else if (auto *movedFailed = lt::alert_cast<lt::storage_moved_failed_alert>(alert))
	{
		addLogEntry("storage", std::string("Storage move failed for '") + movedFailed->torrent_name() + "': " + movedFailed->error.message(), Utils::LogLevel::Error);
	}
	else if (lt::alert_cast<lt::session_stats_alert>(alert))
	{
		addLogEntry("torrent", "Session stats updated", Utils::LogLevel::Debug);
	}
	else if (auto *added = lt::alert_cast<lt::add_torrent_alert>(alert))
	{
		addLogEntry("torrent", added->error ? std::string("Failed to add torrent: ") + added->error.message() : std::string("Torrent added: ") + added->torrent_name(),
			added->error ? Utils::LogLevel::Error : Utils::LogLevel::Info);
	}
	else if (auto *finished = lt::alert_cast<lt::torrent_finished_alert>(alert))
	{
		addLogEntry("torrent", std::string("Torrent finished: ") + finished->torrent_name(), Utils::LogLevel::Info);
	}
	else if (auto *metadata = lt::alert_cast<lt::metadata_received_alert>(alert))
	{
		addLogEntry("torrent", std::string("Metadata received for: ") + metadata->torrent_name(), Utils::LogLevel::Info);
	}
	else if (auto *peerError = lt::alert_cast<lt::peer_error_alert>(alert))
	{
		addLogEntry("peer", std::string("Peer error for '") + peerError->torrent_name() + "': " + peerError->error.message(), Utils::LogLevel::Warning);
	}
	else if (lt::alert_cast<lt::dht_bootstrap_alert>(alert))
	{
		addLogEntry("torrent", "DHT bootstrap complete", Utils::LogLevel::Info);
	}
	else
	{
		const std::string message = alert->message();
		if (!message.empty())
			addLogEntry("torrent", message, message.find("error") != std::string::npos ? Utils::LogLevel::Warning : Utils::LogLevel::Debug);
	}
}
} // namespace Presentation

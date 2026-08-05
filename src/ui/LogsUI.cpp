#include "LogsUI.hpp"
#include "TorrentManager.hpp"
#include "SystemUtils.hpp"
#include "Logger.hpp"
#include <iomanip>
#include <sstream>
#include <ctime>

LogsUI::LogsUI(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void LogsUI::displayLogsWindow()
{
	ImGui::Begin("Logs");

	const auto diagnostics = Utils::Logger::recent();

	// Top toolbar with controls
	if (ImGui::Button("Clear"))
	{
		clearLogs();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &autoScroll);
	
	ImGui::SameLine();
	ImGui::Text("|");
	ImGui::SameLine();
	
	// Filter checkboxes
	ImGui::Checkbox("Debug", &showDebug);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &showInfo);
	ImGui::SameLine();
	ImGui::Checkbox("Warnings", &showWarnings);
	ImGui::SameLine();
	ImGui::Checkbox("Errors", &showErrors);

	ImGui::Separator();

	// Display logs in a scrollable region
	ImGui::BeginChild("LogScrolling", ImVec2(0, -30), false, ImGuiWindowFlags_HorizontalScrollbar);

	const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
	const std::size_t first = diagnostics.size() > maxLogEntries ? diagnostics.size() - maxLogEntries : 0;
	for (std::size_t i = first; i < diagnostics.size(); ++i)
	{
		const auto &record = diagnostics[i];
		if ((record.level == Utils::LogLevel::Debug && !showDebug) ||
			(record.level == Utils::LogLevel::Info && !showInfo) ||
			(record.level == Utils::LogLevel::Warning && !showWarnings) ||
			(record.level == Utils::LogLevel::Error && !showErrors))
			continue;
		const char *level = "INFO";
		ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
		if (record.level == Utils::LogLevel::Debug)
			level = "DEBUG";
		else if (record.level == Utils::LogLevel::Warning)
		{
			level = "WARN";
			color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
		}
		else if (record.level == Utils::LogLevel::Error)
		{
			level = "ERROR";
			color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
		}
		const std::string timestamp = formatTimestamp(record.timestamp);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(("[" + timestamp + "] [" + level + "] [" + record.category + "] " + record.message).c_str());
		ImGui::PopStyleColor();
	}

	if (autoScroll && wasAtBottom)
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();

	// Footer with log count
	ImGui::Separator();
	ImGui::Text("Recent entries: %zu / %zu", diagnostics.size(), maxLogEntries);

	ImGui::End();
}

void LogsUI::updateLogs()
{
	// Poll alerts from TorrentManager
	std::vector<lt::alert *> alerts = torrentManager.pollAlerts();
	
	for (lt::alert *alert : alerts)
	{
		if (alert)
		{
			processAlert(alert);
		}
	}
}

void LogsUI::clearLogs()
{
	Utils::Logger::clearRecent();
}

void LogsUI::setMaxLogEntries(size_t maxEntries)
{
	maxLogEntries = maxEntries;
}

void LogsUI::addLogEntry(const std::string &category, const std::string &message, Utils::LogLevel level)
{
	Utils::Logger::log(level, category, message);
}

std::string LogsUI::formatTimestamp(const std::chrono::system_clock::time_point &time) const
{
	auto time_t = std::chrono::system_clock::to_time_t(time);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
	
	std::stringstream ss;

	std::tm tm_buf = {};
	if (Utils::SystemUtils::getLocalTime(time_t, tm_buf))
	{
		ss << std::put_time(&tm_buf, "%H:%M:%S");
	}
	else
	{
		ss << "00:00:00";
	}

	ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
	
	return ss.str();
}

void LogsUI::processAlert(lt::alert *alert)
{
	if (!alert)
		return;

	// Process tracker errors
	if (auto *te = lt::alert_cast<lt::tracker_error_alert>(alert))
	{
		std::stringstream ss;
		ss << "Tracker error for '" << te->torrent_name() << "': " << te->error_message();
		addLogEntry("tracker", ss.str(), Utils::LogLevel::Error);
	}
	// Process tracker warnings
	else if (auto *tw = lt::alert_cast<lt::tracker_warning_alert>(alert))
	{
		std::stringstream ss;
		ss << "Tracker warning for '" << tw->torrent_name() << "': " << tw->warning_message();
		addLogEntry("tracker", ss.str(), Utils::LogLevel::Warning);
	}
	// Process storage errors
	else if (auto *fe = lt::alert_cast<lt::file_error_alert>(alert))
	{
		std::stringstream ss;
		ss << "File error for '" << fe->torrent_name() << "': " << fe->error.message();
		addLogEntry("storage", ss.str(), Utils::LogLevel::Error);
	}
	else if (auto *smf = lt::alert_cast<lt::storage_moved_failed_alert>(alert))
	{
		std::stringstream ss;
		ss << "Storage move failed for '" << smf->torrent_name() << "': " << smf->error.message();
		addLogEntry("storage", ss.str(), Utils::LogLevel::Error);
	}
	// Process session stats
	else if (auto *stats = lt::alert_cast<lt::session_stats_alert>(alert))
	{
		// Extract some useful stats
		const auto &values = stats->counters();
		std::stringstream msg;
		msg << "Session stats updated";
		addLogEntry("torrent", msg.str(), Utils::LogLevel::Debug);
	}
	// Process torrent added
	else if (auto *ta = lt::alert_cast<lt::add_torrent_alert>(alert))
	{
		if (ta->error)
		{
			std::stringstream ss;
			ss << "Failed to add torrent: " << ta->error.message();
			addLogEntry("torrent", ss.str(), Utils::LogLevel::Error);
		}
		else
		{
			std::stringstream ss;
			ss << "Torrent added: " << ta->torrent_name();
			addLogEntry("torrent", ss.str());
		}
	}
	// Process torrent finished
	else if (auto *tf = lt::alert_cast<lt::torrent_finished_alert>(alert))
	{
		std::stringstream ss;
		ss << "Torrent finished: " << tf->torrent_name();
		addLogEntry("torrent", ss.str());
	}
	// Process metadata received (for magnet links)
	else if (auto *mr = lt::alert_cast<lt::metadata_received_alert>(alert))
	{
		std::stringstream ss;
		ss << "Metadata received for: " << mr->torrent_name();
		addLogEntry("torrent", ss.str());
	}
	// Process peer errors
	else if (auto *pe = lt::alert_cast<lt::peer_error_alert>(alert))
	{
		std::stringstream ss;
		ss << "Peer error for '" << pe->torrent_name() << "': " << pe->error.message();
		addLogEntry("peer", ss.str(), Utils::LogLevel::Warning);
	}
	// Process DHT events
	else if (auto *dht_bootstrap = lt::alert_cast<lt::dht_bootstrap_alert>(alert))
	{
		addLogEntry("torrent", "DHT bootstrap complete");
	}
	// Catch other alerts as general
	else
	{
		// For debugging, log any other alert type
		std::string msg = alert->message();
		if (!msg.empty() && msg.find("error") != std::string::npos)
		{
			addLogEntry("torrent", msg, Utils::LogLevel::Warning);
		}
		else if (!msg.empty())
		{
			addLogEntry("torrent", msg, Utils::LogLevel::Debug);
		}
	}
}

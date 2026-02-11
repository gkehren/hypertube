#include "LogsUI.hpp"
#include "TorrentManager.hpp"
#include "SystemUtils.hpp"
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
	ImGui::Checkbox("Tracker", &showTrackerErrors);
	ImGui::SameLine();
	ImGui::Checkbox("Storage", &showStorageErrors);
	ImGui::SameLine();
	ImGui::Checkbox("Stats", &showConnectionStats);
	ImGui::SameLine();
	ImGui::Checkbox("General", &showGeneralAlerts);

	ImGui::Separator();

	// Display logs in a scrollable region
	ImGui::BeginChild("LogScrolling", ImVec2(0, -30), false, ImGuiWindowFlags_HorizontalScrollbar);

	for (const auto &entry : logEntries)
	{
		// Apply filters
		if ((entry.category == "Tracker" && !showTrackerErrors) ||
			(entry.category == "Storage" && !showStorageErrors) ||
			(entry.category == "Stats" && !showConnectionStats) ||
			(entry.category == "General" && !showGeneralAlerts))
		{
			continue;
		}

		// Display timestamp, category, and message
		std::string timestamp = formatTimestamp(entry.timestamp);
		ImGui::PushStyleColor(ImGuiCol_Text, entry.color);
		ImGui::TextWrapped("[%s] [%s] %s", timestamp.c_str(), entry.category.c_str(), entry.message.c_str());
		ImGui::PopStyleColor();
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();

	// Footer with log count
	ImGui::Separator();
	ImGui::Text("Total entries: %zu / %zu", logEntries.size(), maxLogEntries);

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
	logEntries.clear();
}

void LogsUI::setMaxLogEntries(size_t maxEntries)
{
	maxLogEntries = maxEntries;
}

void LogsUI::addLogEntry(const std::string &category, const std::string &message, const ImVec4 &color)
{
	logEntries.emplace_back(category, message, color);
	
	// Remove oldest entries if we exceed the max
	while (logEntries.size() > maxLogEntries)
	{
		logEntries.pop_front();
	}
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
		addLogEntry("Tracker", ss.str(), ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
	}
	// Process tracker warnings
	else if (auto *tw = lt::alert_cast<lt::tracker_warning_alert>(alert))
	{
		std::stringstream ss;
		ss << "Tracker warning for '" << tw->torrent_name() << "': " << tw->warning_message();
		addLogEntry("Tracker", ss.str(), ImVec4(1.0f, 0.8f, 0.4f, 1.0f)); // Yellow
	}
	// Process storage errors
	else if (auto *fe = lt::alert_cast<lt::file_error_alert>(alert))
	{
		std::stringstream ss;
		ss << "File error for '" << fe->torrent_name() << "': " << fe->error.message();
		addLogEntry("Storage", ss.str(), ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
	}
	else if (auto *smf = lt::alert_cast<lt::storage_moved_failed_alert>(alert))
	{
		std::stringstream ss;
		ss << "Storage move failed for '" << smf->torrent_name() << "': " << smf->error.message();
		addLogEntry("Storage", ss.str(), ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
	}
	// Process session stats
	else if (auto *stats = lt::alert_cast<lt::session_stats_alert>(alert))
	{
		// Extract some useful stats
		const auto &values = stats->counters();
		std::stringstream msg;
		msg << "Session stats updated";
		addLogEntry("Stats", msg.str(), ImVec4(0.6f, 0.8f, 1.0f, 1.0f)); // Light blue
	}
	// Process torrent added
	else if (auto *ta = lt::alert_cast<lt::add_torrent_alert>(alert))
	{
		if (ta->error)
		{
			std::stringstream ss;
			ss << "Failed to add torrent: " << ta->error.message();
			addLogEntry("General", ss.str(), ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
		}
		else
		{
			std::stringstream ss;
			ss << "Torrent added: " << ta->torrent_name();
			addLogEntry("General", ss.str(), ImVec4(0.6f, 1.0f, 0.6f, 1.0f)); // Green
		}
	}
	// Process torrent finished
	else if (auto *tf = lt::alert_cast<lt::torrent_finished_alert>(alert))
	{
		std::stringstream ss;
		ss << "Torrent finished: " << tf->torrent_name();
		addLogEntry("General", ss.str(), ImVec4(0.6f, 1.0f, 0.6f, 1.0f)); // Green
	}
	// Process metadata received (for magnet links)
	else if (auto *mr = lt::alert_cast<lt::metadata_received_alert>(alert))
	{
		std::stringstream ss;
		ss << "Metadata received for: " << mr->torrent_name();
		addLogEntry("General", ss.str(), ImVec4(0.8f, 0.8f, 1.0f, 1.0f)); // Light purple
	}
	// Process peer errors
	else if (auto *pe = lt::alert_cast<lt::peer_error_alert>(alert))
	{
		std::stringstream ss;
		ss << "Peer error for '" << pe->torrent_name() << "': " << pe->error.message();
		addLogEntry("General", ss.str(), ImVec4(1.0f, 0.6f, 0.4f, 1.0f)); // Orange
	}
	// Process DHT events
	else if (auto *dht_bootstrap = lt::alert_cast<lt::dht_bootstrap_alert>(alert))
	{
		addLogEntry("General", "DHT bootstrap complete", ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
	}
	// Catch other alerts as general
	else
	{
		// For debugging, log any other alert type
		std::string msg = alert->message();
		if (!msg.empty() && msg.find("error") != std::string::npos)
		{
			addLogEntry("General", msg, ImVec4(1.0f, 0.6f, 0.4f, 1.0f)); // Orange
		}
		else if (!msg.empty())
		{
			addLogEntry("General", msg, ImVec4(0.8f, 0.8f, 0.8f, 1.0f)); // Gray
		}
	}
}

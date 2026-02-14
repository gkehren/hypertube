#pragma once

#include <imgui.h>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include <string>
#include <vector>
#include <deque>
#include <chrono>

class TorrentManager;

struct LogEntry
{
	std::chrono::system_clock::time_point timestamp;
	std::string category;
	std::string message;
	ImVec4 color;

	LogEntry(const std::string &cat, const std::string &msg, const ImVec4 &col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
		: timestamp(std::chrono::system_clock::now()), category(cat), message(msg), color(col) {}
};

class LogsUI
{
public:
	LogsUI(TorrentManager &torrentManager);
	~LogsUI() = default;

	// Main display method
	void displayLogsWindow();

	// Update logs from libtorrent alerts
	void updateLogs();

	// Clear all logs
	void clearLogs();

	// Set maximum number of log entries to keep
	void setMaxLogEntries(size_t maxEntries);

private:
	TorrentManager &torrentManager;
	std::deque<LogEntry> logEntries;
	std::vector<const LogEntry*> m_filteredEntries;
	size_t maxLogEntries = 1000; // Default max entries
	bool autoScroll = true;

	// Filter settings
	bool showTrackerErrors = true;
	bool showStorageErrors = true;
	bool showConnectionStats = true;
	bool showGeneralAlerts = true;

	// Helper methods
	void addLogEntry(const std::string &category, const std::string &message, const ImVec4 &color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	std::string formatTimestamp(const std::chrono::system_clock::time_point &time) const;
	void processAlert(lt::alert *alert);
};

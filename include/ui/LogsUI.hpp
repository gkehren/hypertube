#pragma once

#include <imgui.h>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include <string>
#include <vector>
#include <chrono>
#include "Logger.hpp"

class TorrentManager;

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
	size_t maxLogEntries = 1000; // Default max entries
	bool autoScroll = true;

	// Filter settings
	bool showDebug = false;
	bool showInfo = true;
	bool showWarnings = true;
	bool showErrors = true;

	// Helper methods
	void addLogEntry(const std::string &category, const std::string &message, Utils::LogLevel level = Utils::LogLevel::Info);
	std::string formatTimestamp(const std::chrono::system_clock::time_point &time) const;
	void processAlert(lt::alert *alert);
};

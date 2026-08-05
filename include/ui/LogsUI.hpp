#pragma once

#include <imgui.h>
#include "Logger.hpp"
#include "presentation/LogsPresenter.hpp"

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
	Presentation::LogsPresenter presenter;
	size_t maxLogEntries = 1000; // Default max entries
	bool autoScroll = true;

	// Filter settings
	bool showDebug = false;
	bool showInfo = true;
	bool showWarnings = true;
	bool showErrors = true;

};

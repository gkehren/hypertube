#include "LogsUI.hpp"
#include "TorrentManager.hpp"
#include "Logger.hpp"

LogsUI::LogsUI(TorrentManager &torrentManager)
	: presenter(torrentManager)
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
	ImGui::Checkbox("Debug", &showDebug);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &showInfo);
	ImGui::SameLine();
	ImGui::Checkbox("Warnings", &showWarnings);
	ImGui::SameLine();
	ImGui::Checkbox("Errors", &showErrors);
	presenter.setLevelEnabled(Utils::LogLevel::Debug, showDebug);
	presenter.setLevelEnabled(Utils::LogLevel::Info, showInfo);
	presenter.setLevelEnabled(Utils::LogLevel::Warning, showWarnings);
	presenter.setLevelEnabled(Utils::LogLevel::Error, showErrors);
	const auto diagnostics = presenter.buildRows();

	ImGui::Separator();

	// Display logs in a scrollable region
	ImGui::BeginChild("LogScrolling", ImVec2(0, -30), false, ImGuiWindowFlags_HorizontalScrollbar);

	const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
	for (const auto &record : diagnostics)
	{
		const char *level = record.level.c_str();
		ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
		if (record.severity == static_cast<int>(Utils::LogLevel::Warning))
		{
			color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
		}
		else if (record.severity == static_cast<int>(Utils::LogLevel::Error))
		{
			color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
		}
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(("[" + record.timestamp + "] [" + level + "] [" + record.category + "] " + record.message).c_str());
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
	presenter.update();
}

void LogsUI::clearLogs()
{
	presenter.clear();
}

void LogsUI::setMaxLogEntries(size_t maxEntries)
{
	maxLogEntries = maxEntries;
	presenter.setMaxEntries(maxEntries);
}

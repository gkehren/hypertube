#include "TorrentDetailsUI.hpp"
#include "TorrentManager.hpp"
#include "StringUtils.hpp"
#include "presentation/UiFormatters.hpp"
#include "SystemUtils.hpp"
#include "Theme.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

TorrentDetailsUI::TorrentDetailsUI(TorrentManager &torrentManager, Utils::SystemUtils::SystemOpener &systemOpener)
	: torrentManager(torrentManager), systemOpener(systemOpener)
{
}

void TorrentDetailsUI::displayTorrentDetails(const lt::torrent_handle &selectedTorrent)
{
	ImGui::Begin("Torrent Details");

	if (selectedTorrent.is_valid())
	{
		// Try to get cached status
		std::optional<lt::torrent_status> cachedStatus = torrentManager.getCachedStatus(selectedTorrent.info_hashes());
		if (!cachedStatus)
		{
			ImGui::TextDisabled("Loading torrent details...");
			ImGui::End();
			return;
		}
		const lt::torrent_status &status = *cachedStatus;

		// Display torrent name as header
		ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().primary);
		ImGui::TextWrapped("%s", status.name.c_str());
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::BeginTabBar("TorrentDetailsTabBar", ImGuiTabBarFlags_FittingPolicyScroll))
		{
			if (ImGui::BeginTabItem("General"))
			{
				displayTorrentDetails_General(status);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Files"))
			{
				displayTorrentDetails_Files(selectedTorrent.info_hashes());
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Peers"))
			{
				displayTorrentDetails_Peers(selectedTorrent.info_hashes());
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Trackers"))
			{
				displayTorrentDetails_Trackers(selectedTorrent.info_hashes());
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Settings"))
			{
				displayTorrentDetails_Settings(selectedTorrent);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	else
	{
		// Centered empty state message
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 textSize = ImGui::CalcTextSize("Select a torrent to view details");
		ImGui::SetCursorPos(ImVec2((windowSize.x - textSize.x) * 0.5f, windowSize.y * 0.4f));
		ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().textSecondary);
		ImGui::Text("Select a torrent to view details");
		ImGui::PopStyleColor();
	}

	ImGui::End();
}

void TorrentDetailsUI::displayTorrentDetails_General(const lt::torrent_status &status)
{
	displayTorrentDetailsContent(status);
}

void TorrentDetailsUI::displayTorrentDetails_Files(const lt::info_hash_t &hash)
{
	torrentManager.requestDetailsRefresh(hash, TorrentDetailSection::Files);
	const auto snapshot = torrentManager.getDetailsSnapshot(hash, TorrentDetailSection::Files);
	if (!snapshot || snapshot->state == TorrentDetailState::Loading)
	{
		ImGui::TextDisabled("Loading file details...");
		return;
	}
	if (snapshot->state != TorrentDetailState::Ready)
	{
		ImGui::TextDisabled("%s", snapshot->message.empty() ? "File metadata is not available." : snapshot->message.c_str());
		return;
	}
	if (snapshot->truncated)
		ImGui::TextDisabled("Some files are not shown because the torrent contains too many entries.");

	if (ImGui::BeginTable("Files", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size");
		ImGui::TableSetupColumn("Progress");
		ImGui::TableSetupColumn("Priority");
		ImGui::TableSetupColumn("Preview");
		ImGui::TableHeadersRow();

		const auto &palette = HypertubeTheme::getCurrentPalette();

		for (const auto &file : snapshot->files)
		{
			ImGui::TableNextRow();

			// Column 0: File Name
			ImGui::TableSetColumnIndex(0);
			const std::string &fileName = file.name;
			ImGui::Text("%s", fileName.c_str());

			// Column 1: Size
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", formatBytes(file.size, false).c_str());

			// Column 2: Progress
			ImGui::TableSetColumnIndex(2);
			float progress = 0.0f;
			if (file.size > 0)
				progress = static_cast<float>(file.downloaded) / static_cast<float>(file.size);
			progress = std::clamp(progress, 0.0f, 1.0f);

			// Color code based on completion
			ImVec4 progressColor;
			if (progress >= 1.0f)
				progressColor = palette.success;
			else if (progress > 0.0f)
				progressColor = palette.progressDownload;
			else
				progressColor = palette.surface;

			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
			char progressText[16];
			snprintf(progressText, sizeof(progressText), "%.0f%%", progress * 100.0f);
			ImGui::ProgressBar(progress, ImVec2(-1, 0), progressText);
			ImGui::PopStyleColor();

			// Column 3: Priority
			ImGui::TableSetColumnIndex(3);
			int priority_index;
			if (file.priority == static_cast<int>(lt::dont_download))
				priority_index = 0;
			else if (file.priority == static_cast<int>(lt::low_priority))
				priority_index = 1;
			else if (file.priority >= static_cast<int>(lt::top_priority))
				priority_index = 3;
			else
				priority_index = 2; // default_priority

			const char* priority_items[] = { "Don't Download", "Low", "Normal", "High" };
			ImGui::SetNextItemWidth(120.0f);
			std::string combo_id = "##priority" + std::to_string(file.index);
			if (ImGui::Combo(combo_id.c_str(), &priority_index, priority_items, IM_ARRAYSIZE(priority_items)))
			{
				// Set the new priority
				int new_priority;
				switch (priority_index)
				{
					case 0: new_priority = static_cast<int>(lt::dont_download); break;
					case 1: new_priority = static_cast<int>(lt::low_priority); break;
					case 2: new_priority = static_cast<int>(lt::default_priority); break;
					case 3: new_priority = static_cast<int>(lt::top_priority); break;
					default: new_priority = static_cast<int>(lt::default_priority); break;
				}
				const Result result = torrentManager.setFilePriority(hash, file.index, new_priority);
				if (!result && onResult)
					onResult(result);
			}

			// Column 4: Preview button
			ImGui::TableSetColumnIndex(4);

			// Check if file is previewable
			bool canPreview = Utils::SystemUtils::isPreviewableFile(fileName);

			if (canPreview)
			{
				// Construct full file path
				std::filesystem::path fullPath = std::filesystem::path(snapshot->savePath) / file.relativePath;

				std::string button_id = "Preview##" + std::to_string(file.index);

				bool hasProgress = progress > 0.0f;

				if (!hasProgress)
				{
					ImGui::BeginDisabled();
				}

				if (ImGui::Button(button_id.c_str()))
				{
					// Enable sequential download if not already enabled
					// Increase priority of this file to ensure faster download
					const Result sequentialResult = torrentManager.executeCommand(hash, TorrentCommand::EnableSequential);
					if (!sequentialResult && onResult)
						onResult(sequentialResult);
					if (file.priority < static_cast<int>(lt::top_priority))
					{
						const Result priorityResult = torrentManager.setFilePriority(hash, file.index, static_cast<int>(lt::top_priority));
						if (!priorityResult && onResult)
							onResult(priorityResult);
					}

					// Open the file for preview
					const Result openResult = systemOpener.enqueuePreview(fullPath.string());
					if (!openResult && onResult)
						onResult(openResult);
				}

				if (!hasProgress)
				{
					ImGui::EndDisabled();
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					{
						ImGui::SetTooltip("File download not started yet");
					}
				}
				else
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Preview with default application (file may still be downloading)");
					}
				}
			}
		}

		ImGui::EndTable();
	}
}

void TorrentDetailsUI::displayTorrentDetails_Peers(const lt::info_hash_t &hash)
{
	torrentManager.requestDetailsRefresh(hash, TorrentDetailSection::Peers);
	const auto snapshot = torrentManager.getDetailsSnapshot(hash, TorrentDetailSection::Peers);
	if (!snapshot || snapshot->state == TorrentDetailState::Loading)
	{
		ImGui::TextDisabled("Loading peer details...");
		return;
	}
	if (snapshot->state != TorrentDetailState::Ready)
	{
		ImGui::TextDisabled("%s", snapshot->message.empty() ? "Peer information is not available." : snapshot->message.c_str());
		return;
	}
	if (snapshot->truncated)
		ImGui::TextDisabled("Peer list truncated at 2000 entries.");

	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Show peer count
	ImGui::TextColored(palette.textSecondary, "Connected Peers: %d", (int)snapshot->peers.size());
	ImGui::Spacing();

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg |
								 ImGuiTableFlags_Resizable |
								 ImGuiTableFlags_ScrollY |
								 ImGuiTableFlags_BordersInnerV;

	if (ImGui::BeginTable("Peers", 5, tableFlags))
	{
		ImGui::TableSetupColumn("IP", ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableSetupColumn("Client", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Down Speed", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Up Speed", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableHeadersRow();

		for (const auto &peer : snapshot->peers)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", peer.address.c_str());

			ImGui::TableSetColumnIndex(1);
			// Safely display peer client by limiting length and handling non-printable chars
			std::string client = peer.client;
			if (client.length() > 12)
			{
				client = client.substr(0, 12);
			}
			// Replace any non-printable characters with '.'
			for (char &c : client)
			{
				if (c < 32 || c > 126)
				{
					c = '.';
				}
			}
			ImGui::Text("%s", client.c_str());

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", peer.flags.c_str());

			ImGui::TableSetColumnIndex(3);
			if (peer.downloadSpeed > 0)
				ImGui::TextColored(palette.success, "%s", formatBytes(peer.downloadSpeed, true).c_str());
			else
				ImGui::Text("%s", formatBytes(peer.downloadSpeed, true).c_str());

			ImGui::TableSetColumnIndex(4);
			if (peer.uploadSpeed > 0)
				ImGui::TextColored(palette.info, "%s", formatBytes(peer.uploadSpeed, true).c_str());
			else
				ImGui::Text("%s", formatBytes(peer.uploadSpeed, true).c_str());
		}

		ImGui::EndTable();
	}
}

void TorrentDetailsUI::displayTorrentDetails_Trackers(const lt::info_hash_t &hash)
{
	torrentManager.requestDetailsRefresh(hash, TorrentDetailSection::Trackers);
	const auto snapshot = torrentManager.getDetailsSnapshot(hash, TorrentDetailSection::Trackers);
	if (!snapshot || snapshot->state == TorrentDetailState::Loading)
	{
		ImGui::TextDisabled("Loading tracker details...");
		return;
	}
	if (snapshot->state != TorrentDetailState::Ready)
	{
		ImGui::TextDisabled("%s", snapshot->message.empty() ? "Tracker information is not available." : snapshot->message.c_str());
		return;
	}
	if (snapshot->truncated)
		ImGui::TextDisabled("Some trackers are not shown because the list is too large.");

	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Show tracker count
	ImGui::TextColored(palette.textSecondary, "Trackers: %d", (int)snapshot->trackers.size());
	ImGui::Spacing();

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg |
								 ImGuiTableFlags_Resizable |
								 ImGuiTableFlags_ScrollY |
								 ImGuiTableFlags_BordersInnerV;

	if (ImGui::BeginTable("Trackers", 2, tableFlags))
	{
		ImGui::TableSetupColumn("URL", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableHeadersRow();

		for (const auto &tracker : snapshot->trackers)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", tracker.url.c_str());

			ImGui::TableSetColumnIndex(1);
			if (tracker.verified)
				ImGui::TextColored(palette.success, "Verified");
			else
				ImGui::TextColored(palette.textSecondary, "Not Verified");
		}

		ImGui::EndTable();
	}
}

void TorrentDetailsUI::displayTorrentDetailsContent(const lt::torrent_status &status)
{
	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Create a two-column layout for details
	if (ImGui::BeginTable("DetailsTable", 2, ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// Size
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Size:");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", formatBytes(status.total_wanted, false).c_str());

		// Progress with colored bar
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Progress:");
		ImGui::TableSetColumnIndex(1);
		ImVec4 progressColor = (status.state == lt::torrent_status::seeding) ? palette.progressUpload : palette.progressDownload;
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
		const std::string progressText = Presentation::UiFormatters::formatProgress(status.progress);
		ImGui::ProgressBar(status.progress, ImVec2(-1, 0), progressText.c_str());
		ImGui::PopStyleColor();

		// Status
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Status:");
		ImGui::TableSetColumnIndex(1);
		const std::string statusText = torrentStateToString(status.state, status.flags);
		const char *statusStr = statusText.c_str();
		ImVec4 statusColor = HypertubeTheme::getStatusColor(statusStr);
		ImGui::TextColored(statusColor, "%s", statusStr);

		// Download Speed
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Down Speed:");
		ImGui::TableSetColumnIndex(1);
		if (status.download_payload_rate > 0)
			ImGui::TextColored(palette.success, "%s", formatBytes(status.download_payload_rate, true).c_str());
		else
			ImGui::Text("%s", formatBytes(status.download_payload_rate, true).c_str());

		// Upload Speed
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Up Speed:");
		ImGui::TableSetColumnIndex(1);
		if (status.upload_payload_rate > 0)
			ImGui::TextColored(palette.info, "%s", formatBytes(status.upload_payload_rate, true).c_str());
		else
			ImGui::Text("%s", formatBytes(status.upload_payload_rate, true).c_str());

		// ETA
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "ETA:");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", computeETA(status).c_str());

		// Seeds/Peers
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Seeds/Peers:");
		ImGui::TableSetColumnIndex(1);
		float ratio = status.num_peers > 0 ? (float)status.num_seeds / (float)status.num_peers : 0.0f;
		ImVec4 ratioColor = HypertubeTheme::getHealthColor(ratio);
		ImGui::TextColored(ratioColor, "%d / %d", status.num_seeds, status.num_peers);

		// Downloaded/Uploaded
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Downloaded:");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", formatBytes(status.total_done, false).c_str());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Uploaded:");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", formatBytes(status.all_time_upload, false).c_str());

		// Save Path
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Save Path:");
		ImGui::TableSetColumnIndex(1);
		ImGui::TextWrapped("%s", status.save_path.c_str());

		ImGui::EndTable();
	}
}

std::string TorrentDetailsUI::formatBytes(size_t bytes, bool speed)
{
	return Presentation::UiFormatters::formatBytes(static_cast<std::int64_t>(bytes), speed);
}

std::string TorrentDetailsUI::torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
{
	return Presentation::UiFormatters::torrentStateToString(
		static_cast<int>(state),
		(flags & lt::torrent_flags::paused) != lt::torrent_flags_t{},
		state == lt::torrent_status::finished);
}

std::string TorrentDetailsUI::computeETA(const lt::torrent_status &status) const
{
	const std::int64_t remaining = std::max<std::int64_t>(0, status.total_wanted - status.total_wanted_done);
	const std::int64_t etaSeconds = status.state == lt::torrent_status::downloading && status.download_payload_rate > 0
		? remaining / status.download_payload_rate : -1;
	return Presentation::UiFormatters::formatEta(etaSeconds);
}

void TorrentDetailsUI::displayTorrentDetails_Settings(const lt::torrent_handle &selectedTorrent)
{
	if (!selectedTorrent.is_valid())
		return;

	ImGui::Text("Per-Torrent Settings");
	ImGui::Separator();
	ImGui::Spacing();

	// Check if we're viewing a different torrent - if so, reload settings
	lt::sha1_hash currentHash = selectedTorrent.info_hash();
	if (settingsState.lastTorrentHash != currentHash)
	{
		settingsState.lastTorrentHash = currentHash;
		// Convert from bytes/s to KB/s (note: this truncates to KB/s granularity)
		settingsState.downloadLimit = selectedTorrent.download_limit() / 1024;
		settingsState.uploadLimit = selectedTorrent.upload_limit() / 1024;
	}

	// Speed limits section
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Speed Limits:");
	ImGui::Spacing();

	// Download limit
	ImGui::Text("Download Limit (KB/s):");
	ImGui::SetNextItemWidth(150);
	if (ImGui::InputInt("##TorrentDownloadLimit", &settingsState.downloadLimit, 1, 100))
	{
		if (settingsState.downloadLimit < 0)
			settingsState.downloadLimit = 0;
		selectedTorrent.set_download_limit(settingsState.downloadLimit * 1024);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(0 = unlimited)");

	ImGui::Spacing();

	// Upload limit
	ImGui::Text("Upload Limit (KB/s):");
	ImGui::SetNextItemWidth(150);
	if (ImGui::InputInt("##TorrentUploadLimit", &settingsState.uploadLimit, 1, 100))
	{
		if (settingsState.uploadLimit < 0)
			settingsState.uploadLimit = 0;
		selectedTorrent.set_upload_limit(settingsState.uploadLimit * 1024);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(0 = unlimited)");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Sequential download section
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Download Mode:");
	ImGui::Spacing();

	bool sequentialMode = (selectedTorrent.flags() & lt::torrent_flags::sequential_download) != lt::torrent_flags_t{};
	if (ImGui::Checkbox("Sequential Download", &sequentialMode))
	{
		if (sequentialMode)
			selectedTorrent.set_flags(lt::torrent_flags::sequential_download);
		else
			selectedTorrent.unset_flags(lt::torrent_flags::sequential_download);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("Sequential mode downloads pieces in order,");
		ImGui::Text("useful for streaming video files.");
		ImGui::EndTooltip();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Info text
	ImGui::TextWrapped("Note: Changes to speed limits and sequential mode are applied immediately.");
}

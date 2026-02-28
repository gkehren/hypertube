#include "TorrentTableUI.hpp"
#include "UIManager.hpp"
#include "StringUtils.hpp"
#include "SystemUtils.hpp"
#include "Theme.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <optional>

TorrentTableUI::TorrentTableUI(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void TorrentTableUI::displayTorrentTable()
{
	// Refresh cache if needed before rendering
	if (torrentManager.shouldRefreshCache())
	{
		torrentManager.refreshStatusCache();
	}

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg |
								 ImGuiTableFlags_Resizable |
								 ImGuiTableFlags_Reorderable |
								 ImGuiTableFlags_Hideable |
								 ImGuiTableFlags_BordersInnerV |
								 ImGuiTableFlags_BordersOuterV |
								 ImGuiTableFlags_ScrollY |
								 ImGuiTableFlags_SizingStretchProp;

	if (ImGui::BeginTable("Torrents", 11, tableFlags))
	{
		displayTorrentTableHeader();
		displayTorrentTableBody();
		ImGui::EndTable();
	}
}

void TorrentTableUI::displayTorrentTableHeader()
{
	ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 35.0f);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
	ImGui::TableSetupColumn("Seeds", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("Peers", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("Down Speed", ImGuiTableColumnFlags_WidthFixed, 90.0f);
	ImGui::TableSetupColumn("Up Speed", ImGuiTableColumnFlags_WidthFixed, 90.0f);
	ImGui::TableSetupColumn("ETA", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn("Ratio", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableHeadersRow();
}

void TorrentTableUI::displayTorrentTableBody()
{
	auto &torrents = torrentManager.getTorrents();

	// Update cache
	m_torrentListCache.clear();
	m_torrentListCache.reserve(torrents.size());
	for (const auto &pair : torrents)
	{
		m_torrentListCache.push_back(&pair);
	}

	ImGuiListClipper clipper;
	clipper.Begin(m_torrentListCache.size());

	// Get status cache snapshot once
	auto statusCache = torrentManager.getStatusCache();

	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			const auto &[info_hash, handle] = *m_torrentListCache[i];

			const lt::torrent_status* statusPtr = nullptr;
			if (statusCache)
			{
				auto it = statusCache->find(info_hash);
				if (it != statusCache->end())
				{
					statusPtr = &it->second;
				}
			}

			displayTorrentTableRow(handle, info_hash, statusPtr);
		}
	}
}

void TorrentTableUI::displayTorrentTableRow(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash, const lt::torrent_status *cachedStatus)
{
	const lt::torrent_status *statusPtr = cachedStatus;
	std::optional<lt::torrent_status> liveStatus;

	if (!statusPtr)
	{
		// Fallback to live query if cache miss (shouldn't happen normally)
		liveStatus.emplace(handle.status());
		statusPtr = &(*liveStatus);
	}

	const lt::torrent_status &status = *statusPtr;

	ImGui::PushID(&handle);
	ImGui::TableNextRow(ImGuiTableRowFlags_None, 26.0f); // Slightly smaller row height

	// Determine if this row is selected
	bool isSelected = (selectedTorrent == handle);

	// Get status string for coloring
	const char *statusStr = Utils::torrentStateToString(status.state, handle.flags());
	ImVec4 statusColor = HypertubeTheme::getStatusColor(statusStr);
	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Column 0: Queue position
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d", static_cast<int>(status.queue_position) + 1);

	// Column 1: Name - Use selectable for row selection
	ImGui::TableSetColumnIndex(1);
	ImGui::AlignTextToFramePadding();
	if (ImGui::Selectable(status.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
	{
		selectedTorrent = handle;
	}

	displayTorrentContextMenu(handle, info_hash);

	// Column 2: Size
	ImGui::TableSetColumnIndex(2);
	ImGui::AlignTextToFramePadding();
	char sizeBuf[64];
	Utils::formatBytes(status.total_wanted, false, sizeBuf, sizeof(sizeBuf));
	ImGui::Text("%s", sizeBuf);

	// Column 3: Progress bar
	ImGui::TableSetColumnIndex(3);
	float progressBarHeight = 14.0f;
	float rowHeight = 26.0f;
	float progressVerticalPadding = (rowHeight - progressBarHeight) * 0.5f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + progressVerticalPadding);

	ImVec4 progressColor;
	if (status.state == lt::torrent_status::seeding)
		progressColor = palette.progressUpload;
	else
		progressColor = palette.progressDownload;

	char progressText[32];
	snprintf(progressText, sizeof(progressText), "%.1f%%", status.progress * 100.0f);

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, palette.progressBackground);
	ImGui::ProgressBar(status.progress, ImVec2(-1, progressBarHeight), progressText);
	ImGui::PopStyleColor(2);

	// Column 4: Status
	ImGui::TableSetColumnIndex(4);
	ImGui::AlignTextToFramePadding();
	ImGui::TextColored(statusColor, "%s", statusStr);

	// Column 5: Seeds
	ImGui::TableSetColumnIndex(5);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d (%d)", status.num_seeds, status.num_complete);

	// Column 6: Peers
	ImGui::TableSetColumnIndex(6);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d (%d)", status.num_peers - status.num_seeds, status.num_incomplete);

	// Column 7: Download speed
	ImGui::TableSetColumnIndex(7);
	ImGui::AlignTextToFramePadding();
	char downSpeedBuf[64];
	Utils::formatBytes(status.download_payload_rate, true, downSpeedBuf, sizeof(downSpeedBuf));
	if (status.download_payload_rate > 0)
		ImGui::TextColored(palette.success, "%s", downSpeedBuf);
	else
		ImGui::Text("%s", downSpeedBuf);

	// Column 8: Upload speed
	ImGui::TableSetColumnIndex(8);
	ImGui::AlignTextToFramePadding();
	char upSpeedBuf[64];
	Utils::formatBytes(status.upload_payload_rate, true, upSpeedBuf, sizeof(upSpeedBuf));
	if (status.upload_payload_rate > 0)
		ImGui::TextColored(palette.info, "%s", upSpeedBuf);
	else
		ImGui::Text("%s", upSpeedBuf);

	// Column 9: ETA
	ImGui::TableSetColumnIndex(9);
	ImGui::AlignTextToFramePadding();
	char etaBuf[64];
	Utils::computeETA(status, etaBuf, sizeof(etaBuf));
	ImGui::Text("%s", etaBuf);

	// Column 10: Ratio
	ImGui::TableSetColumnIndex(10);
	ImGui::AlignTextToFramePadding();
	float ratio = 0.0f;
	if (status.total_download > 0)
	{
		ratio = static_cast<float>(status.total_upload) / static_cast<float>(status.total_download);
	}
	ImVec4 ratioColor = HypertubeTheme::getHealthColor(ratio);
	ImGui::TextColored(ratioColor, "%.2f", ratio);

	ImGui::PopID();
}

void TorrentTableUI::displayTorrentContextMenu(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash)
{
	if (ImGui::BeginPopupContextItem("##context", ImGuiPopupFlags_MouseButtonRight))
	{
		bool isPaused = handle.flags() & lt::torrent_flags::paused;
		const char *pauseLabel = isPaused ? "Resume" : "Pause";

		// Torrent control section
		if (ImGui::MenuItem("Start", nullptr, false, isPaused))
		{
			handle.resume();
		}
		if (ImGui::MenuItem(pauseLabel))
		{
			if (isPaused)
				handle.resume();
			else
				handle.pause();
		}
		if (ImGui::MenuItem("Force Start"))
		{
			handle.force_recheck();
			handle.resume();
		}

		ImGui::Separator();

		// Queue management section
		if (ImGui::MenuItem("Move Up Queue", "Ctrl+Up"))
		{
			handle.queue_position_up();
		}
		if (ImGui::MenuItem("Move Down Queue", "Ctrl+Down"))
		{
			handle.queue_position_down();
		}
		if (ImGui::MenuItem("Move to Top", "Ctrl+Shift+Up"))
		{
			handle.queue_position_top();
		}
		if (ImGui::MenuItem("Move to Bottom", "Ctrl+Shift+Down"))
		{
			handle.queue_position_bottom();
		}

		ImGui::Separator();

		// Actions section
		if (ImGui::MenuItem("Force Recheck"))
		{
			handle.force_recheck();
		}
		if (ImGui::MenuItem("Force Reannounce"))
		{
			handle.force_reannounce();
		}

		ImGui::Separator();

		// File operations section
		if (ImGui::MenuItem("Open Containing Folder"))
		{
			Utils::SystemUtils::openFileExplorer(handle.status().save_path);
		}
		if (ImGui::MenuItem("Copy Magnet Link"))
		{
			ImGui::SetClipboardText(lt::make_magnet_uri(handle).c_str());
		}

		ImGui::Separator();

		// Remove section
		if (ImGui::BeginMenu("Remove"))
		{
			if (ImGui::MenuItem("Remove Torrent"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT);
			}
			if (ImGui::MenuItem("Remove Torrent and Files"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT_DATA);
			}
			if (ImGui::MenuItem("Remove Torrent and .torrent"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT_FILES);
			}
			if (ImGui::MenuItem("Remove Torrent, Files and .torrent"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT_FILES_AND_DATA);
			}
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
}

void TorrentTableUI::setRemoveTorrentCallback(std::function<void(const lt::sha1_hash &, RemoveTorrentType)> callback)
{
	onRemoveTorrent = callback;
}

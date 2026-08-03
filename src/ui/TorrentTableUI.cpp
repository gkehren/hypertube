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
								 ImGuiTableFlags_ScrollY;

	if (ImGui::BeginTable("Torrents", 9, tableFlags))
	{
		displayTorrentTableHeader();
		displayTorrentTableBody();
		ImGui::EndTable();
	}
}

void TorrentTableUI::displayTorrentTableHeader()
{
	ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Size");
	ImGui::TableSetupColumn("Progress");
	ImGui::TableSetupColumn("Status");
	ImGui::TableSetupColumn("Down Speed");
	ImGui::TableSetupColumn("Up Speed");
	ImGui::TableSetupColumn("ETA");
	ImGui::TableSetupColumn("Seeds/Peers");
	ImGui::TableHeadersRow();
}

void TorrentTableUI::displayTorrentTableBody()
{
	auto torrents = torrentManager.getTorrentSnapshot();
	// Get status cache snapshot once. It is immutable for the duration of this
	// frame, so filtering and rendering use a consistent view.
	auto statusCache = torrentManager.getStatusCache();

	// Update cache
	m_torrentListCache.clear();
	m_torrentListCache.reserve(torrents.size());
	for (const auto &torrent : torrents)
	{
		const lt::torrent_status *status = nullptr;
		if (statusCache)
		{
			auto it = statusCache->find(torrent.hash);
			if (it != statusCache->end())
				status = &it->second;
		}
		if (matchesCategory(torrent, status))
			m_torrentListCache.push_back(torrent);
	}

	if (m_torrentListCache.empty())
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);
		if (categoryFilter == 0)
			ImGui::TextWrapped("No torrents yet. Use File > Add a torrent or paste a magnet link to get started.");
		else
			ImGui::TextWrapped("No torrents match this filter.");
		return;
	}

	ImGuiListClipper clipper;
	clipper.Begin(m_torrentListCache.size());

	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			const auto &torrent = m_torrentListCache[i];
			const auto &info_hash = torrent.hash;
			const auto &handle = torrent.handle;

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

bool TorrentTableUI::matchesCategory(const ManagedTorrent &torrent, const lt::torrent_status *status) const
{
	if (categoryFilter == 0 || !status)
		return categoryFilter == 0;
	if (categoryFilter == 1)
		return status->state == lt::torrent_status::downloading || status->state == lt::torrent_status::downloading_metadata;
	if (categoryFilter == 2)
		return status->state == lt::torrent_status::seeding;
	if (categoryFilter == 3)
		return status->is_finished;
	if (categoryFilter == 4)
		return (torrent.handle.flags() & lt::torrent_flags::paused) != lt::torrent_flags_t{};
	if (categoryFilter == 5)
		return status->download_payload_rate > 0 || status->upload_payload_rate > 0;
	if (categoryFilter == 6)
		return status->download_payload_rate == 0 && status->upload_payload_rate == 0;
	return true;
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
	ImGui::TableNextRow(ImGuiTableRowFlags_None, 28.0f); // Fixed row height for consistency

	// Determine if this row is selected
	bool isSelected = (selectedTorrent == handle);

	// Get status string for coloring
	const char *statusStr = Utils::torrentStateToString(status.state, handle.flags());
	ImVec4 statusColor = HypertubeTheme::getStatusColor(statusStr);
	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Store the row rect for selection detection
	ImGui::TableSetColumnIndex(0);
	float rowStartY = ImGui::GetCursorScreenPos().y;
	float rowMinX = ImGui::GetCursorScreenPos().x;

	// Column 0: Queue position
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
	float progressBarHeight = 16.0f;
	float rowHeight = 28.0f;
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

	// Column 5: Download speed
	ImGui::TableSetColumnIndex(5);
	ImGui::AlignTextToFramePadding();
	char downSpeedBuf[64];
	Utils::formatBytes(status.download_payload_rate, true, downSpeedBuf, sizeof(downSpeedBuf));
	if (status.download_payload_rate > 0)
		ImGui::TextColored(palette.success, "%s", downSpeedBuf);
	else
		ImGui::Text("%s", downSpeedBuf);

	// Column 6: Upload speed
	ImGui::TableSetColumnIndex(6);
	ImGui::AlignTextToFramePadding();
	char upSpeedBuf[64];
	Utils::formatBytes(status.upload_payload_rate, true, upSpeedBuf, sizeof(upSpeedBuf));
	if (status.upload_payload_rate > 0)
		ImGui::TextColored(palette.info, "%s", upSpeedBuf);
	else
		ImGui::Text("%s", upSpeedBuf);

	// Column 7: ETA
	ImGui::TableSetColumnIndex(7);
	ImGui::AlignTextToFramePadding();
	char etaBuf[64];
	Utils::computeETA(status, etaBuf, sizeof(etaBuf));
	ImGui::Text("%s", etaBuf);

	// Column 8: Seeds/Peers
	ImGui::TableSetColumnIndex(8);
	ImGui::AlignTextToFramePadding();
	float ratio = status.num_peers > 0 ? (float)status.num_seeds / (float)status.num_peers : 0.0f;
	ImVec4 ratioColor = HypertubeTheme::getHealthColor(ratio);
	ImGui::TextColored(ratioColor, "%d / %d", status.num_seeds, status.num_peers);

	ImGui::PopID();
}

void TorrentTableUI::displayTorrentContextMenu(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash)
{
	if (ImGui::BeginPopupContextItem("##context", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem("Open"))
		{
		}

		if (ImGui::MenuItem("Open Containing Folder"))
		{
			Utils::SystemUtils::openFileExplorer(handle.status().save_path);
		}

		if (ImGui::MenuItem("Copy Magnet URI"))
		{
			ImGui::SetClipboardText(lt::make_magnet_uri(handle).c_str());
		}

		if (ImGui::MenuItem("Force Start"))
		{
			handle.force_recheck();
			handle.resume();
		}

		if (ImGui::MenuItem("Start"))
		{
			handle.resume();
		}

		if (ImGui::MenuItem("Pause"))
		{
			if (handle.flags() & lt::torrent_flags::paused)
				handle.resume();
			else
				handle.pause();
		}

		if (ImGui::MenuItem("Stop"))
		{
			handle.pause();
			handle.force_recheck();
		}

		if (ImGui::MenuItem("Move Up Queue"))
		{
			handle.queue_position_up();
		}

		if (ImGui::MenuItem("Move Down Queue"))
		{
			handle.queue_position_down();
		}

		bool isSequential = (handle.flags() & lt::torrent_flags::sequential_download) != lt::torrent_flags_t{};
		if (ImGui::MenuItem("Sequential Download (Streaming)", nullptr, isSequential))
		{
			if (isSequential)
				handle.unset_flags(lt::torrent_flags::sequential_download);
			else
				handle.set_flags(lt::torrent_flags::sequential_download);
		}

		if (ImGui::BeginMenu("Remove"))
		{
			if (ImGui::MenuItem("Remove"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT);
			}
			if (ImGui::MenuItem("Remove with Data"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT_DATA);
			}
			if (ImGui::MenuItem("Remove with .torrent"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT_FILES);
			}
			if (ImGui::MenuItem("Remove with Data & .torrent"))
			{
				if (onRemoveTorrent)
					onRemoveTorrent(info_hash, REMOVE_TORRENT_FILES_AND_DATA);
			}
			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Update Tracker"))
		{
			handle.force_reannounce();
		}

		if (ImGui::MenuItem("Properties"))
		{
		}

		ImGui::EndPopup();
	}
}

void TorrentTableUI::setRemoveTorrentCallback(std::function<void(const lt::sha1_hash &, RemoveTorrentType)> callback)
{
	onRemoveTorrent = callback;
}

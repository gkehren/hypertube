#include "TorrentTableUI.hpp"
#include "UIManager.hpp"
#include "StringUtils.hpp"
#include "Theme.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cmath>

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

	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			const auto &[info_hash, handle] = *m_torrentListCache[i];
			displayTorrentTableRow(handle, info_hash);
		}
	}
}

void TorrentTableUI::displayTorrentTableRow(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash)
{
	// Try to get cached status first
	const lt::torrent_status *cachedStatus = torrentManager.getCachedStatus(info_hash);
	lt::torrent_status status;

	if (cachedStatus)
	{
		status = *cachedStatus;
	}
	else
	{
		// Fallback to live query if cache miss (shouldn't happen normally)
		status = handle.status();
	}

	ImGui::PushID(&handle);
	ImGui::TableNextRow();

	char buf[128];
	const char *cell_text = buf;

	// Determine if this row is selected
	bool isSelected = (selectedTorrent == handle);

	// Get status string for coloring
	const char *statusStr = Utils::torrentStateToString(status.state, handle.flags());
	ImVec4 statusColor = HypertubeTheme::getStatusColor(statusStr);

	for (int col = 0; col < 9; col++)
	{
		ImGui::TableSetColumnIndex(col);

		// Optimization: Construct text into buffer directly to avoid std::string allocation
		buf[0] = '\0';	 // clear buffer
		cell_text = buf; // Default to buffer

		switch (col)
		{
		case 0: // Queue position
			snprintf(buf, sizeof(buf), "%d", static_cast<int>(status.queue_position) + 1);
			break;
		case 1: // Name
			cell_text = status.name.c_str();
			break;
		case 2: // Size
			Utils::formatBytes(status.total_wanted, false, buf, sizeof(buf));
			break;
		case 4: // Status
			cell_text = statusStr;
			break;
		case 5: // Download speed
			Utils::formatBytes(status.download_payload_rate, true, buf, sizeof(buf));
			break;
		case 6: // Upload speed
			Utils::formatBytes(status.upload_payload_rate, true, buf, sizeof(buf));
			break;
		case 7: // ETA
			Utils::computeETA(status, buf, sizeof(buf));
			break;
		case 8: // Seeds/Peers
		{
			float ratio = status.num_peers > 0 ? (float)status.num_seeds / (float)status.num_peers : 0.0f;
			snprintf(buf, sizeof(buf), "%d / %d", status.num_seeds, status.num_peers);
		}
		break;
		case 3:
			// Handled separately below
			break;
		default:
			break;
		}

		if (col == 3) // Progress column with colored bar
		{
			// Choose progress bar color based on download/seeding state
			ImVec4 progressColor;
			if (status.state == lt::torrent_status::seeding)
				progressColor = HypertubeTheme::getCurrentPalette().progressUpload;
			else
				progressColor = HypertubeTheme::getCurrentPalette().progressDownload;

			// Draw progress bar
			char progressText[32];
			snprintf(progressText, sizeof(progressText), "%.1f%%", status.progress * 100.0f);

			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
			ImGui::ProgressBar(status.progress, ImVec2(-1, 0), progressText);
			ImGui::PopStyleColor();
		}
		else if (col == 4) // Status column with colored text
		{
			ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
			if (ImGui::Selectable(cell_text, isSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				selectedTorrent = handle;
			}
			ImGui::PopStyleColor();
		}
		else if (col == 5 && status.download_payload_rate > 0) // Download speed with color
		{
			ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().success);
			if (ImGui::Selectable(cell_text, isSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				selectedTorrent = handle;
			}
			ImGui::PopStyleColor();
		}
		else if (col == 6 && status.upload_payload_rate > 0) // Upload speed with color
		{
			ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().info);
			if (ImGui::Selectable(cell_text, isSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				selectedTorrent = handle;
			}
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Selectable(cell_text, isSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				selectedTorrent = handle;
			}
		}

		ImGui::SameLine();
	}

	displayTorrentContextMenu(handle, info_hash);
	ImGui::PopID();
}

std::string TorrentTableUI::getTorrentCellText(const lt::torrent_status &status, int column, const lt::torrent_handle &handle)
{
	// Kept for backward compatibility or other usages, but now calls optimized logic
	char buf[128];
	buf[0] = '\0';

	switch (column)
	{
	case 0: // Queue position
		snprintf(buf, sizeof(buf), "%d", static_cast<int>(status.queue_position) + 1);
		return std::string(buf);
	case 1: // Name
		return status.name;
	case 2: // Size
		Utils::formatBytes(status.total_wanted, false, buf, sizeof(buf));
		return std::string(buf);
	case 4: // Status
		return std::string(Utils::torrentStateToString(status.state, handle.flags()));
	case 5: // Download speed
		Utils::formatBytes(status.download_payload_rate, true, buf, sizeof(buf));
		return std::string(buf);
	case 6: // Upload speed
		Utils::formatBytes(status.upload_payload_rate, true, buf, sizeof(buf));
		return std::string(buf);
	case 7: // ETA
		Utils::computeETA(status, buf, sizeof(buf));
		return std::string(buf);
	case 8: // Seeds/Peers
	{
		float ratio = status.num_peers > 0 ? (float)status.num_seeds / (float)status.num_peers : 0.0f;
		snprintf(buf, sizeof(buf), "%d/%d (%f)", status.num_seeds, status.num_peers, ratio);
		return std::string(buf);
	}
	default:
		return "";
	}
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
#ifdef _WIN32
			std::string command = "explorer.exe \"" + handle.status().save_path + "\"";
#elif __APPLE__
			std::string command = "open \"" + handle.status().save_path + "\"";
#elif __linux__
			std::string command = "xdg-open \"" + handle.status().save_path + "\"";
#endif
			int ret = system(command.c_str());
			if (ret != 0)
			{
				std::cerr << "Failed to open containing folder" << std::endl;
			}
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

std::string TorrentTableUI::torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
{
	return std::string(Utils::torrentStateToString(state, flags));
}

std::string TorrentTableUI::formatBytes(size_t bytes, bool speed)
{
	char buf[64];
	Utils::formatBytes(bytes, speed, buf, sizeof(buf));
	return std::string(buf);
}

std::string TorrentTableUI::computeETA(const lt::torrent_status &status) const
{
	char buf[64];
	Utils::computeETA(status, buf, sizeof(buf));
	return std::string(buf);
}

void TorrentTableUI::setRemoveTorrentCallback(std::function<void(const lt::sha1_hash &, RemoveTorrentType)> callback)
{
	onRemoveTorrent = callback;
}

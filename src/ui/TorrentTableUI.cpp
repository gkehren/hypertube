#include "TorrentTableUI.hpp"
#include "UIManager.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cmath>

namespace {
	// Optimized helper functions to avoid std::string allocations
	void formatBytesOptimized(size_t bytes, bool speed, char* buf, size_t buf_size)
	{
		static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
		size_t size = bytes;
		size_t unitIndex = 0;

		while (size >= 1024 && unitIndex < 4)
		{
			size /= 1024;
			unitIndex++;
		}

		snprintf(buf, buf_size, "%zu %s%s", size, units[unitIndex], speed ? "/s" : "");
	}

	const char* torrentStateToStringOptimized(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
	{
		if (flags & lt::torrent_flags::paused)
			return "Paused";

		switch (state)
		{
		case lt::torrent_status::downloading_metadata:
			return "Downloading metadata";
		case lt::torrent_status::downloading:
			return "Downloading";
		case lt::torrent_status::finished:
			return "Finished";
		case lt::torrent_status::seeding:
			return "Seeding";
		case lt::torrent_status::checking_files:
			return "Checking files";
		case lt::torrent_status::checking_resume_data:
			return "Checking resume data";
		default:
			return "Unknown";
		}
	}

	void computeETAOptimized(const lt::torrent_status &status, char* buf, size_t buf_size)
	{
		if (status.state == lt::torrent_status::downloading && status.download_payload_rate > 0)
		{
			int64_t secondsLeft = (status.total_wanted - status.total_wanted_done) / status.download_payload_rate;
			int64_t minutesLeft = secondsLeft / 60;
			int64_t hoursLeft = minutesLeft / 60;
			int64_t daysLeft = hoursLeft / 24;

			if (daysLeft > 0)
				snprintf(buf, buf_size, "%lld days", (long long)daysLeft);
			else if (hoursLeft > 0)
				snprintf(buf, buf_size, "%lld hours", (long long)hoursLeft);
			else if (minutesLeft > 0)
				snprintf(buf, buf_size, "%lld minutes", (long long)minutesLeft);
			else
				snprintf(buf, buf_size, "%lld seconds", (long long)secondsLeft);
		}
		else
		{
			if (buf_size > 3) {
				 buf[0] = 'N'; buf[1] = '/'; buf[2] = 'A'; buf[3] = '\0';
			}
		}
	}
}

TorrentTableUI::TorrentTableUI(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void TorrentTableUI::displayTorrentTable()
{
	if (ImGui::BeginTable("Torrents", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
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
	for (const auto &[info_hash, handle] : torrents)
	{
		displayTorrentTableRow(handle, info_hash);
	}
}

void TorrentTableUI::displayTorrentTableRow(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash)
{
	lt::torrent_status status = handle.status();
	ImGui::PushID(&handle);
	ImGui::TableNextRow();

	char buf[128];
	const char* cell_text = buf;

	for (int col = 0; col < 9; col++)
	{
		ImGui::TableSetColumnIndex(col);

		// Optimization: Construct text into buffer directly to avoid std::string allocation
		buf[0] = '\0'; // clear buffer
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
			formatBytesOptimized(status.total_wanted, false, buf, sizeof(buf));
			break;
		case 4: // Status
			cell_text = torrentStateToStringOptimized(status.state, handle.flags());
			break;
		case 5: // Download speed
			formatBytesOptimized(status.download_payload_rate, true, buf, sizeof(buf));
			break;
		case 6: // Upload speed
			formatBytesOptimized(status.upload_payload_rate, true, buf, sizeof(buf));
			break;
		case 7: // ETA
			computeETAOptimized(status, buf, sizeof(buf));
			break;
		case 8: // Seeds/Peers
			{
				float ratio = status.num_peers > 0 ? (float)status.num_seeds / (float)status.num_peers : 0.0f;
				// Use default float formatting to match std::to_string approximately or just %f
				snprintf(buf, sizeof(buf), "%d/%d (%f)", status.num_seeds, status.num_peers, ratio);
			}
			break;
		case 3:
			// Handled separately below
			break;
		default:
			break;
		}

		if (col != 3) // Progress column is handled differently
		{
			if (ImGui::Selectable(cell_text, selectedTorrent == handle, ImGuiSelectableFlags_SpanAllColumns))
			{
				selectedTorrent = handle;
			}
		}
		else
		{
			ImGui::ProgressBar(status.progress);
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
		formatBytesOptimized(status.total_wanted, false, buf, sizeof(buf));
		return std::string(buf);
	case 4: // Status
		return std::string(torrentStateToStringOptimized(status.state, handle.flags()));
	case 5: // Download speed
		formatBytesOptimized(status.download_payload_rate, true, buf, sizeof(buf));
		return std::string(buf);
	case 6: // Upload speed
		formatBytesOptimized(status.upload_payload_rate, true, buf, sizeof(buf));
		return std::string(buf);
	case 7: // ETA
		computeETAOptimized(status, buf, sizeof(buf));
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
		const std::vector<MenuItem> menuItems = {
			{"Open", "", []() {}},
			{"Open Containing Folder", "", [=]()
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
			 }},
			{"Copy Magnet URI", "", [=]()
			 {
				 ImGui::SetClipboardText(lt::make_magnet_uri(handle).c_str());
			 }},
			{"Force Start", "", [=]()
			 {
				 handle.force_recheck();
				 handle.resume();
			 }},
			{"Start", "", [=]()
			 {
				 handle.resume();
			 }},
			{"Pause", "", [=]()
			 {
				 if (handle.flags() & lt::torrent_flags::paused)
					 handle.resume();
				 else
					 handle.pause();
			 }},
			{"Stop", "", [=]()
			 {
				 handle.pause();
				 handle.force_recheck();
			 }},
			{"Move Up Queue", "", [=]()
			 {
				 handle.queue_position_up();
			 }},
			{"Move Down Queue", "", [=]()
			 {
				 handle.queue_position_down();
			 }},
			{"Remove", "", [=]()
			 {
				 if (onRemoveTorrent)
					 onRemoveTorrent(info_hash, REMOVE_TORRENT);
			 }},
			{"Update Tracker", "", [=]()
			 {
				 handle.force_reannounce();
			 }},
			{"Properties", "", []() {}},
		};

		for (const auto &item : menuItems)
		{
			if (ImGui::MenuItem(item.label.c_str(), item.shortcut.c_str()))
			{
				item.action();
			}
		}
		ImGui::EndPopup();
	}
}

std::string TorrentTableUI::torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
{
	return std::string(torrentStateToStringOptimized(state, flags));
}

std::string TorrentTableUI::formatBytes(size_t bytes, bool speed)
{
	char buf[64];
	formatBytesOptimized(bytes, speed, buf, sizeof(buf));
	return std::string(buf);
}

std::string TorrentTableUI::computeETA(const lt::torrent_status &status) const
{
	char buf[64];
	computeETAOptimized(status, buf, sizeof(buf));
	return std::string(buf);
}

void TorrentTableUI::setRemoveTorrentCallback(std::function<void(const lt::sha1_hash &, RemoveTorrentType)> callback)
{
	onRemoveTorrent = callback;
}

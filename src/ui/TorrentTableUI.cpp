#include "TorrentTableUI.hpp"
#include "UIManager.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

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

	for (int col = 0; col < 9; col++)
	{
		ImGui::TableSetColumnIndex(col);
		std::string cell_text = getTorrentCellText(status, col, handle);

		if (col != 3) // Progress column is handled differently
		{
			if (ImGui::Selectable(cell_text.c_str(), selectedTorrent == handle, ImGuiSelectableFlags_SpanAllColumns))
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
	switch (column)
	{
	case 0: // Queue position
		return std::to_string(static_cast<int>(status.queue_position) + 1);
	case 1: // Name
		return status.name;
	case 2: // Size
		return formatBytes(status.total_wanted, false);
	case 4: // Status
		return torrentStateToString(status.state, handle.flags());
	case 5: // Download speed
		return formatBytes(status.download_payload_rate, true);
	case 6: // Upload speed
		return formatBytes(status.upload_payload_rate, true);
	case 7: // ETA
		return computeETA(status);
	case 8: // Seeds/Peers
		return std::to_string(status.num_seeds) + "/" + std::to_string(status.num_peers) +
			   " (" + std::to_string(status.num_seeds / (float)status.num_peers) + ")";
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

std::string TorrentTableUI::formatBytes(size_t bytes, bool speed)
{
	const char *units[] = {"B", "KB", "MB", "GB", "TB"};
	size_t size = bytes;
	size_t unitIndex = 0;

	while (size >= 1024 && unitIndex < sizeof(units) / sizeof(units[0]) - 1)
	{
		size /= 1024;
		unitIndex++;
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
	if (speed)
		oss << "/s";
	return oss.str();
}

std::string TorrentTableUI::computeETA(const lt::torrent_status &status) const
{
	if (status.state == lt::torrent_status::downloading && status.download_payload_rate > 0)
	{
		int64_t secondsLeft = (status.total_wanted - status.total_wanted_done) / status.download_payload_rate;
		int64_t minutesLeft = secondsLeft / 60;
		int64_t hoursLeft = minutesLeft / 60;
		int64_t daysLeft = hoursLeft / 24;

		if (daysLeft > 0)
			return std::to_string(daysLeft) + " days";
		if (hoursLeft > 0)
			return std::to_string(hoursLeft) + " hours";
		if (minutesLeft > 0)
			return std::to_string(minutesLeft) + " minutes";
		return std::to_string(secondsLeft) + " seconds";
	}
	return "N/A";
}

void TorrentTableUI::setRemoveTorrentCallback(std::function<void(const lt::sha1_hash &, RemoveTorrentType)> callback)
{
	onRemoveTorrent = callback;
}
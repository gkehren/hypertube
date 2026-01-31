#include "TorrentDetailsUI.hpp"
#include "TorrentManager.hpp"
#include "StringUtils.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

TorrentDetailsUI::TorrentDetailsUI(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void TorrentDetailsUI::displayTorrentDetails(const lt::torrent_handle &selectedTorrent)
{
	ImGui::Begin("Torrent Details");

	if (selectedTorrent.is_valid())
	{
		// Try to get cached status
		const lt::torrent_status *cachedStatus = torrentManager.getCachedStatus(selectedTorrent.info_hash());
		lt::torrent_status status;

		if (cachedStatus)
		{
			status = *cachedStatus;
		}
		else
		{
			// Fallback to live query
			status = selectedTorrent.status();
		}

		if (ImGui::BeginTabBar("TorrentDetailsTabBar"))
		{
			if (ImGui::BeginTabItem("General"))
			{
				displayTorrentDetails_General(status, selectedTorrent);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Files"))
			{
				displayTorrentDetails_Files(selectedTorrent);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Peers"))
			{
				displayTorrentDetails_Peers(selectedTorrent);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Trackers"))
			{
				displayTorrentDetails_Trackers(selectedTorrent);
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
		ImGui::Text("No torrent selected");
	}

	ImGui::End();
}

void TorrentDetailsUI::displayTorrentDetails_General(const lt::torrent_status &status, const lt::torrent_handle &handle)
{
	displayTorrentDetailsContent(status, handle);
}

void TorrentDetailsUI::displayTorrentDetails_Files(const lt::torrent_handle &selectedTorrent)
{
	if (!selectedTorrent.is_valid())
		return;

	auto torrentFile = selectedTorrent.torrent_file();
	if (!torrentFile)
	{
		ImGui::Text("Metadata not available yet.");
		return;
	}

	auto file_storage = torrentFile->files();
	std::vector<std::int64_t> file_progress;
	selectedTorrent.file_progress(file_progress);

	if (ImGui::BeginTable("Files", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size");
		ImGui::TableSetupColumn("Progress");
		ImGui::TableSetupColumn("Priority");
		ImGui::TableHeadersRow();

		for (int i = 0; i < file_storage.num_files(); ++i)
		{
			lt::file_index_t index(i);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", file_storage.file_name(index).to_string().c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", formatBytes(file_storage.file_size(index), false).c_str());

			ImGui::TableSetColumnIndex(2);
			if (file_storage.file_size(index) > 0)
				ImGui::ProgressBar(static_cast<float>(file_progress[i]) / file_storage.file_size(index));
			else
				ImGui::ProgressBar(0.0f);

			ImGui::TableSetColumnIndex(3);
			// Get current priority
			lt::download_priority_t current_priority = selectedTorrent.file_priority(index);
			int priority_index = 0;
			if (current_priority == lt::dont_download) priority_index = 0;
			else if (current_priority == lt::low_priority) priority_index = 1;
			else if (current_priority == lt::default_priority) priority_index = 2;
			else if (current_priority >= lt::top_priority) priority_index = 3;

			const char* priority_items[] = { "Don't Download", "Low", "Normal", "High" };
			ImGui::SetNextItemWidth(120.0f);
			std::string combo_id = "##priority" + std::to_string(i);
			if (ImGui::Combo(combo_id.c_str(), &priority_index, priority_items, IM_ARRAYSIZE(priority_items)))
			{
				// Set the new priority
				lt::download_priority_t new_priority;
				switch (priority_index)
				{
					case 0: new_priority = lt::dont_download; break;
					case 1: new_priority = lt::low_priority; break;
					case 2: new_priority = lt::default_priority; break;
					case 3: new_priority = lt::top_priority; break;
					default: new_priority = lt::default_priority; break;
				}
				selectedTorrent.file_priority(index, new_priority);
			}
		}

		ImGui::EndTable();
	}
}

void TorrentDetailsUI::displayTorrentDetails_Peers(const lt::torrent_handle &selectedTorrent)
{
	if (!selectedTorrent.is_valid())
		return;

	std::vector<lt::peer_info> peers;
	selectedTorrent.get_peer_info(peers);

	if (ImGui::BeginTable("Peers", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("IP");
		ImGui::TableSetupColumn("Client");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableSetupColumn("Down Speed");
		ImGui::TableSetupColumn("Up Speed");
		ImGui::TableHeadersRow();

		for (const auto &peer : peers)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", peer.ip.address().to_string().c_str());

			ImGui::TableSetColumnIndex(1);
			// Safely display peer client by limiting length and handling non-printable chars
			std::string client = peer.client;
			if (client.length() > 8)
			{
				client = client.substr(0, 8);
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
			char flagsBuf[32];
			Utils::getPeerFlags(peer, flagsBuf, sizeof(flagsBuf));
			ImGui::Text("%s", flagsBuf);

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%s", formatBytes(peer.payload_down_speed, true).c_str());

			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%s", formatBytes(peer.payload_up_speed, true).c_str());
		}

		ImGui::EndTable();
	}
}

void TorrentDetailsUI::displayTorrentDetails_Trackers(const lt::torrent_handle &selectedTorrent)
{
	if (!selectedTorrent.is_valid())
		return;

	auto trackers = selectedTorrent.trackers();

	if (ImGui::BeginTable("Trackers", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("URL");
		ImGui::TableSetupColumn("Status");
		ImGui::TableHeadersRow();

		for (const auto &tracker : trackers)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", tracker.url.c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", tracker.verified ? "Verified" : "Not Verified");
		}

		ImGui::EndTable();
	}
}

void TorrentDetailsUI::displayTorrentDetailsContent(const lt::torrent_status &status, const lt::torrent_handle &handle)
{
	const std::vector<std::pair<std::string, std::function<std::string()>>> details = {
		{"Name", [&]()
		 { return status.name; }},
		{"Size", [&]()
		 { return formatBytes(status.total_wanted, false); }},
		{"Progress", [&]()
		 { return std::to_string(status.progress * 100) + "%%"; }},
		{"Status", [&]()
		 { return torrentStateToString(status.state, handle.flags()); }},
		{"Down Speed", [&]()
		 { return formatBytes(status.download_payload_rate, true); }},
		{"Up Speed", [&]()
		 { return formatBytes(status.upload_payload_rate, true); }},
		{"ETA", [&]()
		 { return computeETA(status); }},
		{"Seeds/Peers", [&]()
		 { return std::to_string(status.num_seeds) + "/" + std::to_string(status.num_peers) +
				  " (" + std::to_string(status.num_seeds / (float)status.num_peers) + ")"; }},
	};

	for (const auto &detail : details)
	{
		ImGui::Text("%s: %s", detail.first.c_str(), detail.second().c_str());
	}
}

std::string TorrentDetailsUI::formatBytes(size_t bytes, bool speed)
{
	char buf[64];
	Utils::formatBytes(bytes, speed, buf, sizeof(buf));
	return std::string(buf);
}

std::string TorrentDetailsUI::torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
{
	return std::string(Utils::torrentStateToString(state, flags));
}

std::string TorrentDetailsUI::computeETA(const lt::torrent_status &status) const
{
	char buf[64];
	Utils::computeETA(status, buf, sizeof(buf));
	return std::string(buf);
}

void TorrentDetailsUI::displayTorrentDetails_Settings(const lt::torrent_handle &selectedTorrent)
{
	if (!selectedTorrent.is_valid())
		return;

	ImGui::Text("Per-Torrent Settings");
	ImGui::Separator();
	ImGui::Spacing();

	// Speed limits section
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Speed Limits:");
	ImGui::Spacing();

	// Get current limits (convert from bytes/s to KB/s)
	int downloadLimit = selectedTorrent.download_limit() / 1024;
	int uploadLimit = selectedTorrent.upload_limit() / 1024;

	// Download limit
	ImGui::Text("Download Limit (KB/s):");
	ImGui::SetNextItemWidth(150);
	if (ImGui::InputInt("##TorrentDownloadLimit", &downloadLimit, 1, 100))
	{
		if (downloadLimit < 0)
			downloadLimit = 0;
		selectedTorrent.set_download_limit(downloadLimit * 1024);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(0 = unlimited)");

	ImGui::Spacing();

	// Upload limit
	ImGui::Text("Upload Limit (KB/s):  ");
	ImGui::SetNextItemWidth(150);
	if (ImGui::InputInt("##TorrentUploadLimit", &uploadLimit, 1, 100))
	{
		if (uploadLimit < 0)
			uploadLimit = 0;
		selectedTorrent.set_upload_limit(uploadLimit * 1024);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(0 = unlimited)");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Sequential download section
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Download Mode:");
	ImGui::Spacing();

	bool sequentialMode = (selectedTorrent.flags() & lt::torrent_flags::sequential_download);
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

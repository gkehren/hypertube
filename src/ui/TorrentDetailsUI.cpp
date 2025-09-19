#include "TorrentDetailsUI.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

void TorrentDetailsUI::displayTorrentDetails(const lt::torrent_handle &selectedTorrent)
{
	ImGui::Begin("Torrent Details");

	if (selectedTorrent.is_valid())
	{
		lt::torrent_status status = selectedTorrent.status();

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

	auto file_storage = selectedTorrent.torrent_file()->files();
	std::vector<std::int64_t> file_progress;
	selectedTorrent.file_progress(file_progress);

	if (ImGui::BeginTable("Files", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size");
		ImGui::TableSetupColumn("Progress");
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
			// TODO: Display flags
			ImGui::Text("TODO");

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

std::string TorrentDetailsUI::torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
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

std::string TorrentDetailsUI::computeETA(const lt::torrent_status &status) const
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
#include "TorrentDetailsUI.hpp"
#include "TorrentManager.hpp"
#include "StringUtils.hpp"
#include "Theme.hpp"
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
		ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().textSecondary);
		ImGui::Text("Metadata not available yet.");
		ImGui::PopStyleColor();
		return;
	}

	auto file_storage = torrentFile->files();
	std::vector<std::int64_t> file_progress;
	selectedTorrent.file_progress(file_progress);

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg |
								 ImGuiTableFlags_Resizable |
								 ImGuiTableFlags_ScrollY |
								 ImGuiTableFlags_BordersInnerV;

	if (ImGui::BeginTable("Files", 3, tableFlags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableHeadersRow();

		const auto &palette = HypertubeTheme::getCurrentPalette();

		for (int i = 0; i < file_storage.num_files(); ++i)
		{
			lt::file_index_t index(i);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", file_storage.file_name(index).to_string().c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", formatBytes(file_storage.file_size(index), false).c_str());

			ImGui::TableSetColumnIndex(2);
			float progress = 0.0f;
			if (file_storage.file_size(index) > 0)
				progress = static_cast<float>(file_progress[i]) / file_storage.file_size(index);

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

	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Show peer count
	ImGui::TextColored(palette.textSecondary, "Connected Peers: %d", (int)peers.size());
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

		for (const auto &peer : peers)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", peer.ip.address().to_string().c_str());

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
			char flagsBuf[32];
			Utils::getPeerFlags(peer, flagsBuf, sizeof(flagsBuf));
			ImGui::Text("%s", flagsBuf);

			ImGui::TableSetColumnIndex(3);
			if (peer.payload_down_speed > 0)
				ImGui::TextColored(palette.success, "%s", formatBytes(peer.payload_down_speed, true).c_str());
			else
				ImGui::Text("%s", formatBytes(peer.payload_down_speed, true).c_str());

			ImGui::TableSetColumnIndex(4);
			if (peer.payload_up_speed > 0)
				ImGui::TextColored(palette.info, "%s", formatBytes(peer.payload_up_speed, true).c_str());
			else
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

	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Show tracker count
	ImGui::TextColored(palette.textSecondary, "Trackers: %d", (int)trackers.size());
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

		for (const auto &tracker : trackers)
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

void TorrentDetailsUI::displayTorrentDetailsContent(const lt::torrent_status &status, const lt::torrent_handle &handle)
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
		char progressText[32];
		snprintf(progressText, sizeof(progressText), "%.1f%%", status.progress * 100.0f);
		ImGui::ProgressBar(status.progress, ImVec2(-1, 0), progressText);
		ImGui::PopStyleColor();

		// Status
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextColored(palette.textSecondary, "Status:");
		ImGui::TableSetColumnIndex(1);
		const char *statusStr = Utils::torrentStateToString(status.state, handle.flags());
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

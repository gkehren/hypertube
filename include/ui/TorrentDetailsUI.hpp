#pragma once

#include <imgui.h>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <string>
#include <functional>

class TorrentDetailsUI
{
public:
	TorrentDetailsUI() = default;
	~TorrentDetailsUI() = default;

	// Main display method
	void displayTorrentDetails(const lt::torrent_handle &selectedTorrent);

	// Tab display methods
	void displayTorrentDetails_General(const lt::torrent_status &status, const lt::torrent_handle &handle);
	void displayTorrentDetails_Files(const lt::torrent_handle &selectedTorrent);
	void displayTorrentDetails_Peers(const lt::torrent_handle &selectedTorrent);
	void displayTorrentDetails_Trackers(const lt::torrent_handle &selectedTorrent);

	// Utility methods
	void displayTorrentDetailsContent(const lt::torrent_status &status, const lt::torrent_handle &handle);

	// Formatting utilities (shared with other UI classes)
	std::string formatBytes(size_t bytes, bool speed);
	std::string torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags);
	std::string computeETA(const lt::torrent_status &status) const;

private:
	// No private members needed for this stateless UI class
};
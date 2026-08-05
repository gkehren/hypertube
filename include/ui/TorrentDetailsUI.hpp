#pragma once

#include <imgui.h>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/info_hash.hpp>
#include <string>
#include <functional>
#include <utility>
#include "Result.hpp"
#include "SystemUtils.hpp"

class TorrentManager;

class TorrentDetailsUI
{
public:
	TorrentDetailsUI(TorrentManager &torrentManager, Utils::SystemUtils::SystemOpener &systemOpener);
	~TorrentDetailsUI() = default;

	// Main display method
	void displayTorrentDetails(const lt::torrent_handle &selectedTorrent);
	void setResultCallback(std::function<void(const Result &)> callback) { onResult = std::move(callback); }

	// Tab display methods
	void displayTorrentDetails_General(const lt::torrent_status &status);
	void displayTorrentDetails_Files(const lt::info_hash_t &hash);
	void displayTorrentDetails_Peers(const lt::info_hash_t &hash);
	void displayTorrentDetails_Trackers(const lt::info_hash_t &hash);
	void displayTorrentDetails_Settings(const lt::torrent_handle &selectedTorrent);

	// Utility methods
	void displayTorrentDetailsContent(const lt::torrent_status &status);

	// Formatting utilities (shared with other UI classes)
	std::string formatBytes(size_t bytes, bool speed);
	std::string torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags);
	std::string computeETA(const lt::torrent_status &status) const;

private:
	TorrentManager &torrentManager;
	Utils::SystemUtils::SystemOpener &systemOpener;
	
	// Settings tab state (per-torrent hash to settings map)
	struct SettingsState
	{
		int downloadLimit = 0;
		int uploadLimit = 0;
		lt::sha1_hash lastTorrentHash;
	};
	SettingsState settingsState;
	std::function<void(const Result &)> onResult;
};

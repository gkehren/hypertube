#pragma once

#include <imgui.h>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/torrent_status.hpp>
#include <string>
#include <functional>
#include "TorrentManager.hpp"

struct TorrentRemovalInfo;

struct MenuItem
{
	std::string label;
	std::string shortcut;
	std::function<void()> action;
};

class TorrentTableUI
{
public:
	TorrentTableUI(TorrentManager &torrentManager);
	~TorrentTableUI() = default;

	// Main table display methods
	void displayTorrentTable();
	void displayTorrentTableHeader();
	void displayTorrentTableBody();
	void displayTorrentTableRow(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash, const lt::torrent_status *cachedStatus);

	// Utility methods for torrent display
	std::string getTorrentCellText(const lt::torrent_status &status, int column, const lt::torrent_handle &handle);
	void displayTorrentContextMenu(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash);

	// Formatting utilities
	std::string torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags);
	std::string formatBytes(size_t bytes, bool speed);
	std::string computeETA(const lt::torrent_status &status) const;

	// Selection management
	lt::torrent_handle getSelectedTorrent() const { return selectedTorrent; }
	void setSelectedTorrent(const lt::torrent_handle &handle) { selectedTorrent = handle; }

	// Callback setup for actions that need to be handled by parent
	void setRemoveTorrentCallback(std::function<void(const lt::sha1_hash &, RemoveTorrentType)> callback);

private:
	TorrentManager &torrentManager;
	lt::torrent_handle selectedTorrent;

	// Callback for torrent removal (handled by parent)
	std::function<void(const lt::sha1_hash &, RemoveTorrentType)> onRemoveTorrent;

	// Cache for ImGuiListClipper
	std::vector<const std::pair<const lt::sha1_hash, lt::torrent_handle> *> m_torrentListCache;
};
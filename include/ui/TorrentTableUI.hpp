#pragma once

#include <imgui.h>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/torrent_status.hpp>
#include <string>
#include <functional>
#include "TorrentManager.hpp"
#include "SystemUtils.hpp"
#include "presentation/TorrentListPresenter.hpp"

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
	TorrentTableUI(TorrentManager &torrentManager, Utils::SystemUtils::SystemOpener &systemOpener);
	~TorrentTableUI() = default;

	// Main table display methods
	void displayTorrentTable();
	void displayTorrentTableHeader();
	void displayTorrentTableBody();
	void displayTorrentTableRow(const lt::torrent_handle &handle, const lt::info_hash_t &info_hash, const lt::torrent_status *cachedStatus);

	// Utility methods for torrent display
	void displayTorrentContextMenu(const lt::torrent_handle &handle, const lt::info_hash_t &info_hash);

	// Selection management
	lt::torrent_handle getSelectedTorrent() const { return selectedTorrent; }
	void setSelectedTorrent(const lt::torrent_handle &handle) { selectedTorrent = handle; }

	// Callback setup for actions that need to be handled by parent
	void setRemoveTorrentCallback(std::function<void(const lt::info_hash_t &, const std::string &, TorrentRemovalMode)> callback);
	void setResultCallback(std::function<void(const Result &)> callback);
	void setCategoryFilter(int filter) { presenter.setCategoryFilter(filter); }
	std::vector<Presentation::CategoryDto> getCategories() { return presenter.buildCategories(); }

private:
	TorrentManager &torrentManager;
	Utils::SystemUtils::SystemOpener &systemOpener;
	Presentation::TorrentListPresenter presenter;
	lt::torrent_handle selectedTorrent;

	// Callback for torrent removal (handled by parent)
	std::function<void(const lt::info_hash_t &, const std::string &, TorrentRemovalMode)> onRemoveTorrent;
	std::function<void(const Result &)> onResult;

	// Cache for ImGuiListClipper
	std::vector<Presentation::TorrentRowDto> m_torrentListCache;
};

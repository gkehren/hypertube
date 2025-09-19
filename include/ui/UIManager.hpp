#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuiFileDialog.h>
#include <ImGuiFileDialogConfig.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include "TorrentManager.hpp"
#include "SearchEngine.hpp"
#include "Result.hpp"

struct MenuItem
{
	std::string label;
	std::string shortcut;
	std::function<void()> action;
};

struct TorrentRemovalInfo
{
	lt::sha1_hash hash;
	RemoveTorrentType removeType;

	TorrentRemovalInfo(const lt::sha1_hash &hash, RemoveTorrentType removeType)
		: hash(hash), removeType(removeType) {}
};

class UIManager
{
public:
	UIManager(TorrentManager &torrentManager, SearchEngine &searchEngine);
	~UIManager() = default;

	void init(GLFWwindow *window);
	void initImGui(GLFWwindow *window);
	void setDefaultSavePath();
	std::string torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags);
	std::string formatBytes(size_t bytes, bool speed);
	std::string computeETA(const lt::torrent_status &status) const;
	void renderFrame(GLFWwindow *window, const ImVec4 &clear_color);
	void shutdown();
	const ImGuiIO &getIO() const;
	bool shouldExit() const;

private:
	ImGuiIO io;
	char magnetLinkBuffer[4096] = {0};
	char searchQueryBuffer[256] = {0};
	bool exitRequested = false;
	lt::torrent_handle selectedTorrent;
	bool showFailurePopup = false;
	std::string failurePopupMessage;
	std::string defaultSavePath;
	std::string savePath;

	std::pair<bool, std::string> torrentToAdd;
	std::vector<TorrentRemovalInfo> torrentsToRemove;

	// Search-related members
	std::vector<TorrentSearchResult> searchResults;
	TorrentSearchResult selectedSearchResult;
	bool showSearchWindow = false;
	bool isSearching = false;
	std::string currentSearchQuery;
	std::string nextToken;
	bool hasMoreResults = true;

	TorrentManager &torrentManager;
	SearchEngine &searchEngine;

	void displayCategories();
	void displayTorrentList();
	void displayTorrentManagement();
	void displayTorrentDetails();
	void displayTorrentDetails_General(const lt::torrent_status &status);
	void displayTorrentDetails_Files();
	void displayTorrentDetails_Peers();
	void displayTorrentDetails_Trackers();
	void displayTorrentContextMenu(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash);
	void displayTorrentTableHeader();
	void displayTorrentTableBody();
	void displayTorrentTableRow(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash);
	std::string getTorrentCellText(const lt::torrent_status &status, int column, const lt::torrent_handle &handle);
	void displayTorrentDetailsContent(const lt::torrent_status &status);
	void handleAddTorrentModal(bool &showTorrentPopup);
	void handleAddMagnetTorrentModal(bool &showMagnetTorrentPopup);
	void handleRemoveTorrentModal();
	void handleAskSavePathModal();
	void displayMenuBar(bool &showTorrentPopup, bool &showMagnetTorrentPopup);
	void setupDocking(ImGuiID dockspace_id);

	// Search-related methods
	void displaySearchWindow();
	void displayIntegratedSearch();
	void displaySearchResults();
	void displayEnhancedSearchResults();
	void displaySearchResultRow(const TorrentSearchResult &result, int index);
	void displayEnhancedSearchResultRow(const TorrentSearchResult &result, int index);
	void handleSearchResultSelection(const TorrentSearchResult &result);
	void performSearch(const std::string &query);
	void displayPaginationControls();
	void loadMoreResults();

	// Modal Windows
	void askSavePathModal();
	void renderPopupFailure(const std::string &message);
	void removeTorrentModal();
};
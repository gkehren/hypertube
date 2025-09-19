#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include "SearchEngine.hpp"
#include "Result.hpp"

class SearchUI
{
public:
	SearchUI(SearchEngine &searchEngine);
	~SearchUI() = default;

	// Main display methods
	void displayIntegratedSearch();
	void displaySearchWindow();
	void displaySearchResults();
	void displayEnhancedSearchResults();

	// Search result display methods
	void displaySearchResultRow(const TorrentSearchResult &result, int index);
	void displayEnhancedSearchResultRow(const TorrentSearchResult &result, int index);

	// Search functionality
	void performSearch(const std::string &query);
	void loadMoreResults();
	void displayPaginationControls();

	// Selection handling
	void handleSearchResultSelection(const TorrentSearchResult &result);

	// Utility methods
	std::string formatBytes(size_t bytes, bool speed);
	std::string formatUnixTime(int64_t unixTime);

	// Callback setup
	void setSearchResultSelectedCallback(std::function<void(const TorrentSearchResult &)> callback);
	void setShowFailurePopupCallback(std::function<void(const std::string &)> callback);

	// State access
	bool isShowSearchWindow() const { return showSearchWindow; }
	void setShowSearchWindow(bool show) { showSearchWindow = show; }

	const TorrentSearchResult &getSelectedSearchResult() const { return selectedSearchResult; }
	void clearSelectedSearchResult() { selectedSearchResult = TorrentSearchResult(); }

private:
	SearchEngine &searchEngine;

	// Search state
	char searchQueryBuffer[256] = {0};
	std::vector<TorrentSearchResult> searchResults;
	TorrentSearchResult selectedSearchResult;
	bool showSearchWindow = false;
	bool isSearching = false;
	std::string currentSearchQuery;
	std::string nextToken;
	bool hasMoreResults = true;

	// Callbacks
	std::function<void(const TorrentSearchResult &)> onSearchResultSelected;
	std::function<void(const std::string &)> onShowFailurePopup;
};
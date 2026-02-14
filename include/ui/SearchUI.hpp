#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <optional>
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
	void displayFavorites();

	// Search result display methods
	void displaySearchResultRow(const TorrentSearchResult &result, int index);
	void displayEnhancedSearchResultRow(const TorrentSearchResult &result, int index);
	void displayFavoriteRow(const TorrentSearchResult &result, int index);

	// Search functionality
	void performSearch(const std::string &query);
	void loadMoreResults();
	void displayPaginationControls();

	// Selection handling
	void handleSearchResultSelection(const TorrentSearchResult &result);

	// Utility methods
	void formatUnixTime(int64_t unixTime, char *buffer, size_t bufferSize);
	void sortTorrentResults(std::vector<TorrentSearchResult> &results, ImGuiTableSortSpecs *sort_specs);

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
	std::vector<TorrentSearchResult> favoritesDisplay;
	uint64_t lastFavoritesRevision = 0;
	TorrentSearchResult selectedSearchResult;
	bool showSearchWindow = false;
	std::atomic<bool> isSearching;
	std::string currentSearchQuery;
	std::string nextToken;
	bool hasMoreResults = true;

	// Async search state (protected by mutex)
	std::mutex resultsMutex;
	bool hasPendingResults = false;
	SearchResponse pendingResponse;
	std::optional<Result> pendingResult;
	std::string pendingErrorMessage;

	// Callbacks
	std::function<void(const TorrentSearchResult &)> onSearchResultSelected;
	std::function<void(const std::string &)> onShowFailurePopup;

	// Internal methods
	void processPendingResults();
	bool isInFavorites(const std::string &infoHash) const;
};
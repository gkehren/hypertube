#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include "SearchEngine.hpp"
#include "Result.hpp"

class SearchUI
{
public:
	enum class State { Idle, Loading, Results, Empty, Cancelled, Failed };
	SearchUI(SearchEngine &searchEngine);
	~SearchUI() = default;

	// Main display methods
	void displayIntegratedSearch();
	void displayEnhancedSearchResults();
	void displayFavorites();
	void update();

	// Search result display methods
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
	std::atomic<bool> isSearching;
	std::string currentSearchQuery;
	std::string nextToken;
	bool hasMoreResults = true;

	uint64_t activeRequestId = 0;
	bool loadingMore = false;
	State state = State::Idle;
	std::string stateMessage;
	bool resultsChanged = false;

	// Callbacks
	std::function<void(const TorrentSearchResult &)> onSearchResultSelected;
	std::function<void(const std::string &)> onShowFailurePopup;

	// Internal methods
	void processPendingResults();
	void mergeUniqueResults(std::vector<TorrentSearchResult> results);
	bool isInFavorites(const std::string &infoHash) const;
};

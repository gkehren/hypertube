#pragma once

#include "SearchEngine.hpp"
#include "presentation/UiDtos.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Presentation
{
enum class SearchState
{
	Idle,
	Loading,
	Results,
	Empty,
	Cancelled,
	Failed
};

class SearchPresenter
{
public:
	explicit SearchPresenter(SearchEngine &searchEngine);

	Result startSearch(const std::string &query);
	Result loadMore();
	void cancel();
	void update();

	SearchState state() const { return state_; }
	const std::string &stateMessage() const { return stateMessage_; }
	const std::string &query() const { return currentQuery_; }
	const std::string &nextToken() const { return nextToken_; }
	bool hasMore() const { return hasMore_; }
	bool loadingMore() const { return loadingMore_; }
	bool isSearching() const { return state_ == SearchState::Loading; }

	std::vector<SearchResultDto> buildResultRows(bool favorites = false);
	const std::vector<TorrentSearchResult> &results() const { return results_; }
	const std::vector<TorrentSearchResult> &favorites() const { return favorites_; }
	std::optional<TorrentSearchResult> selectedResult() const { return selectedResult_; }
	void selectResult(const TorrentSearchResult &result) { selectedResult_ = result; }

	void addFavorite(const TorrentSearchResult &result);
	void removeFavorite(const std::string &infoHash);
	bool isFavorite(const std::string &infoHash) const;

private:
	SearchEngine &searchEngine;
	std::vector<TorrentSearchResult> results_;
	std::vector<TorrentSearchResult> favorites_;
	std::optional<TorrentSearchResult> selectedResult_;
	std::string currentQuery_;
	std::string nextToken_;
	std::string stateMessage_;
	uint64_t activeRequestId_ = 0;
	uint64_t favoritesRevision_ = 0;
	bool hasMore_ = true;
	bool loadingMore_ = false;
	SearchState state_ = SearchState::Idle;

	void mergeUnique(std::vector<TorrentSearchResult> incoming);
	static SearchResultDto toDto(const TorrentSearchResult &result, bool favorite);
};
} // namespace Presentation

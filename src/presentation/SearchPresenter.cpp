#include "presentation/SearchPresenter.hpp"

#include "presentation/UiFormatters.hpp"

#include <algorithm>

namespace Presentation
{
SearchPresenter::SearchPresenter(SearchEngine &searchEngine)
	: searchEngine(searchEngine)
{
}

Result SearchPresenter::startSearch(const std::string &query)
{
	if (query.empty())
		return Result::Failure("Search query cannot be empty", ResultCode::InvalidInput);
	if (isSearching())
		return Result::Failure("A search is already in progress", ResultCode::Busy, true);

	uint64_t requestId = 0;
	const Result result = searchEngine.startSearch(SearchQuery(query), requestId);
	if (!result)
		return result;

	currentQuery_ = query;
	nextToken_.clear();
	results_.clear();
	hasMore_ = true;
	loadingMore_ = false;
	activeRequestId_ = requestId;
	stateMessage_.clear();
	state_ = SearchState::Loading;
	return Result::Success();
}

Result SearchPresenter::loadMore()
{
	if (isSearching() || currentQuery_.empty() || nextToken_.empty() || !hasMore_)
		return Result::Failure("There are no more search results to load", ResultCode::Unavailable);

	uint64_t requestId = 0;
	const Result result = searchEngine.startSearch(SearchQuery(currentQuery_, 0, nextToken_), requestId);
	if (!result)
		return result;
	activeRequestId_ = requestId;
	loadingMore_ = true;
	state_ = SearchState::Loading;
	stateMessage_.clear();
	return Result::Success();
}

void SearchPresenter::cancel()
{
	if (isSearching())
		searchEngine.cancelCurrentSearch();
}

void SearchPresenter::update()
{
	if (favoritesRevision_ != searchEngine.getFavoritesRevision())
	{
		favorites_ = searchEngine.getFavorites();
		favoritesRevision_ = searchEngine.getFavoritesRevision();
	}

	const auto completion = searchEngine.takeCompletedSearch();
	if (!completion || completion->requestId != activeRequestId_)
		return;

	loadingMore_ = false;
	if (!completion->result)
	{
		if (completion->result.code == ResultCode::Cancelled)
		{
			state_ = SearchState::Cancelled;
			stateMessage_ = "Search cancelled.";
		}
		else
		{
			state_ = SearchState::Failed;
			stateMessage_ = completion->result.message;
		}
		return;
	}

	nextToken_ = completion->response.nextToken;
	hasMore_ = completion->response.hasMore && !nextToken_.empty();
	mergeUnique(completion->response.torrents);
	if (results_.empty())
	{
		state_ = SearchState::Empty;
		stateMessage_ = "No torrents found.";
	}
	else
	{
		state_ = SearchState::Results;
		stateMessage_.clear();
	}
}

void SearchPresenter::mergeUnique(std::vector<TorrentSearchResult> incoming)
{
	for (auto &result : incoming)
	{
		const auto duplicate = std::find_if(results_.begin(), results_.end(), [&result](const TorrentSearchResult &existing)
		{
			return !result.infoHash.empty() && result.infoHash == existing.infoHash;
		});
		if (duplicate == results_.end())
			results_.push_back(std::move(result));
	}
}

SearchResultDto SearchPresenter::toDto(const TorrentSearchResult &result, bool favorite)
{
	SearchResultDto dto;
	dto.id = result.infoHash.empty() ? result.magnetUri : result.infoHash;
	dto.name = result.name;
	dto.sizeBytes = static_cast<std::int64_t>(result.sizeBytes);
	dto.sizeLabel = UiFormatters::formatBytes(dto.sizeBytes);
	dto.seeders = result.seeders;
	dto.leechers = result.leechers;
	dto.seedersLabel = UiFormatters::formatCount(result.seeders);
	dto.leechersLabel = UiFormatters::formatCount(result.leechers);
	dto.ratioLabel = UiFormatters::formatRatio(result.seeders, result.leechers);
	dto.completed = result.completed;
	dto.completedLabel = UiFormatters::formatCount(result.completed);
	dto.createdLabel = UiFormatters::formatUnixDate(result.createdUnix);
	dto.lastSeenLabel = UiFormatters::formatUnixDate(result.scrapedDate);
	dto.category = result.category;
	dto.magnetUri = result.magnetUri;
	dto.favorite = favorite;
	return dto;
}

std::vector<SearchResultDto> SearchPresenter::buildResultRows(bool favorites)
{
	if (favorites && favoritesRevision_ != searchEngine.getFavoritesRevision())
	{
		favorites_ = searchEngine.getFavorites();
		favoritesRevision_ = searchEngine.getFavoritesRevision();
	}
	const auto &source = favorites ? favorites_ : results_;
	std::vector<SearchResultDto> rows;
	rows.reserve(source.size());
	for (const auto &result : source)
		rows.push_back(toDto(result, favorites || searchEngine.isFavorite(result.infoHash)));
	return rows;
}

void SearchPresenter::addFavorite(const TorrentSearchResult &result)
{
	searchEngine.addToFavorites(result);
	favoritesRevision_ = 0;
}

void SearchPresenter::removeFavorite(const std::string &infoHash)
{
	searchEngine.removeFromFavorites(infoHash);
	favoritesRevision_ = 0;
}

bool SearchPresenter::isFavorite(const std::string &infoHash) const
{
	return searchEngine.isFavorite(infoHash);
}
} // namespace Presentation

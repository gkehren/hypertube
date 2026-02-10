#pragma once

#include "Result.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>

struct TorrentSearchResult
{
	std::string name;
	std::string magnetUri;
	std::string infoHash;
	size_t sizeBytes;
	int seeders;
	int leechers;
	std::string dateUploaded;
	std::string category;
	int64_t createdUnix;
	int64_t scrapedDate;
	int completed;

	TorrentSearchResult() = default;
	TorrentSearchResult(const std::string &name, const std::string &magnetUri,
						const std::string &infoHash, size_t sizeBytes, int seeders,
						int leechers, const std::string &dateUploaded, const std::string &category,
						int64_t createdUnix = 0, int64_t scrapedDate = 0, int completed = 0)
		: name(name), magnetUri(magnetUri), infoHash(infoHash),
		  sizeBytes(sizeBytes), seeders(seeders), leechers(leechers),
		  dateUploaded(dateUploaded), category(category), createdUnix(createdUnix),
		  scrapedDate(scrapedDate), completed(completed) {}
};

struct SearchQuery
{
	std::string query;
	int maxResults = 0;			// 0 means use API default (don't send size parameter)
	std::string nextToken = ""; // Token for pagination (from API 'next' field)

	SearchQuery(const std::string &q) : query(q) {}
	SearchQuery(const std::string &q, int max) : query(q), maxResults(max) {}
	SearchQuery(const std::string &q, int max, const std::string &next)
		: query(q), maxResults(max), nextToken(next) {}
};

struct SearchResponse
{
	std::vector<TorrentSearchResult> torrents;
	std::string nextToken;
	bool hasMore = false;
};

class SearchEngine
{
public:
	SearchEngine();
	~SearchEngine();

	// Core search functionality
	Result searchTorrents(const SearchQuery &query, std::vector<TorrentSearchResult> &results);
	Result searchTorrents(const SearchQuery &query, SearchResponse &response);

	// Async search with proper threading
	void searchTorrentsAsync(const SearchQuery &query, std::function<void(Result, SearchResponse)> callback);

	// Search history and favorites
	void addToSearchHistory(const std::string &query);
	const std::vector<std::string> &getSearchHistory() const;
	void clearSearchHistory();

	void addToFavorites(const TorrentSearchResult &result);
	void removeFromFavorites(const std::string &infoHash);
	const std::vector<TorrentSearchResult> &getFavorites() const;
	uint64_t getFavoritesRevision() const { return favoritesRevision; }
	bool isFavorite(const std::string &infoHash) const;

	// Persistence
	void saveFavoritesAndHistory(class ConfigManager &configManager);
	void loadFavoritesAndHistory(class ConfigManager &configManager);

	// Configuration
	void setApiUrl(const std::string &url);
	void setTimeout(int seconds);
	void setMaxRetries(int retries);

	// Status
	bool isSearching() const;
	void cancelCurrentSearch();
	bool isCancellationRequested() const { return cancelRequested.load(); }

private:
	std::string apiUrl;
	int timeoutSeconds;
	int maxRetries;
	std::atomic<bool> searching;
	std::atomic<bool> cancelRequested;

	std::mutex searchMutex;
	std::thread searchThread;

	std::vector<std::string> searchHistory;
	std::vector<TorrentSearchResult> favorites;
	std::atomic<uint64_t> favoritesRevision{0};
	std::unordered_set<std::string> favoriteHashes;
	mutable std::mutex favoritesMutex;

	// HTTP client methods
	Result makeHttpRequest(const std::string &url, std::string &response);
	std::string buildSearchUrl(const SearchQuery &query) const;
	Result parseSearchResponse(const std::string &response, std::vector<TorrentSearchResult> &results);
	Result parseSearchResponse(const std::string &response, SearchResponse &searchResponse);

	// Utility methods
};
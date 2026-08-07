#pragma once

#include "Result.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cstdint>
#include <chrono>
#include <condition_variable>

struct TorrentSearchResult
{
	std::string name;
	std::string magnetUri;
	std::string infoHash;
	size_t sizeBytes = 0;
	int seeders = 0;
	int leechers = 0;
	std::string dateUploaded;
	std::string category;
	int64_t createdUnix = 0;
	int64_t scrapedDate = 0;
	int completed = 0;

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

struct CompletedSearch
{
	uint64_t requestId = 0;
	Result result = Result::Failure("Search did not complete", ResultCode::Internal);
	SearchResponse response;
};

class SearchEngine
{
public:
	friend class SearchEngineTest;
	using SearchProvider = std::function<Result(const SearchQuery &, SearchResponse &, const std::function<bool()> &)>;
	SearchEngine();
	~SearchEngine();

	// Core search functionality
	Result searchTorrents(const SearchQuery &query, std::vector<TorrentSearchResult> &results);
	Result searchTorrents(const SearchQuery &query, SearchResponse &response);
	Result registerSearchProvider(const std::string &id, SearchProvider provider);
	Result setActiveSearchProvider(const std::string &id);
	std::string getActiveSearchProvider() const;
	std::vector<std::string> getSearchProviders() const;
	Result configureTorznabProvider(const std::string &url, const std::string &apiKey = "");
	static Result validateTorznabConfig(const std::string &url);
	void clearSearchCache();

	// Async searches publish owned completions for the UI thread to consume.
	Result startSearch(const SearchQuery &query, uint64_t &requestId);
	std::optional<CompletedSearch> takeCompletedSearch();
	void shutdown();

	// Search history and favorites
	void addToSearchHistory(const std::string &query);
	std::vector<std::string> getSearchHistory() const;
	void clearSearchHistory();

	void addToFavorites(const TorrentSearchResult &result);
	void removeFromFavorites(const std::string &infoHash);
	std::vector<TorrentSearchResult> getFavorites() const;
	uint64_t getFavoritesRevision() const { return favoritesRevision; }
	bool isFavorite(const std::string &infoHash) const;
	std::unordered_set<std::string> getFavoriteHashesSet() const;

	// Persistence
	void saveFavoritesAndHistory(class ConfigManager &configManager);
	void loadFavoritesAndHistory(class ConfigManager &configManager);

	// Configuration
	void setApiUrl(const std::string &url);
	void setTimeout(int seconds);
	void setMaxRetries(int retries);
	Result setProxyConfig(bool enabled, const std::string &type, const std::string &host,
		int port, const std::string &username = "", const std::string &password = "");
	static Result validateProxyConfig(bool enabled, const std::string &type, const std::string &host, int port);

	// Status
	bool isSearching() const;
	void cancelCurrentSearch();
	bool isCancellationRequested() const { return cancelRequested.load(); }

private:
	std::string apiUrl;
	int timeoutSeconds;
	int maxRetries;
	bool proxyEnabled = false;
	std::string proxyType = "socks5";
	std::string proxyHost;
	int proxyPort = 1080;
	std::string proxyUsername;
	std::string proxyPassword;
	std::atomic<bool> searching;
	std::atomic<bool> cancelRequested;
	std::atomic<bool> shuttingDown{false};
	std::atomic<uint64_t> nextRequestId{1};

	std::mutex queueMutex_;
	std::condition_variable queueCv_;
	std::thread workerThread_;
	bool stopWorker_ = false;
	bool hasWork_ = false;
	struct SearchTask
	{
		SearchQuery query{""};
		uint64_t requestId = 0;
	} pendingTask_;

	std::mutex curlMutex_;
	void *curlHandle_ = nullptr;

	std::mutex completionMutex;
	std::optional<CompletedSearch> completedSearch;

	std::vector<std::string> searchHistory;
	std::vector<TorrentSearchResult> favorites;
	std::atomic<uint64_t> favoritesRevision{0};
	mutable std::mutex historyMutex;
	std::unordered_set<std::string> favoriteHashes;
	mutable std::mutex favoritesMutex;
	mutable std::mutex settingsMutex;
	mutable std::mutex providersMutex;
	std::unordered_map<std::string, SearchProvider> providers;
	std::string activeProvider = "torrents-csv";
	struct CachedSearch
	{
		SearchResponse response;
		std::chrono::steady_clock::time_point expiresAt;
	};
	mutable std::mutex cacheMutex;
	std::unordered_map<std::string, CachedSearch> searchCache;

	// HTTP client methods
	Result makeHttpRequest(const std::string &url, std::string &response);
	std::string buildSearchUrl(const SearchQuery &query) const;
	Result parseSearchResponse(const std::string &response, std::vector<TorrentSearchResult> &results);
	Result parseSearchResponse(const std::string &response, SearchResponse &searchResponse);
	Result parseTorznabResponse(const std::string &response, SearchResponse &searchResponse);
	Result performSearch(const SearchQuery &query, SearchResponse &response);
	bool tryStartSearch();
	void finishSearch();
	void workerLoop();
	void cleanupCurlHandle();

	// Utility methods
};

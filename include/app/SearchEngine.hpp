#pragma once

#include "Result.hpp"
#include "SearchProvider.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>

class SearchEngine
{
public:
	friend class SearchEngineTest;
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

	// Provider management
	void setActiveProvider(const std::string &providerName);
	const std::string &getActiveProviderName() const;
	std::vector<std::string> getAvailableProviders() const;
	std::shared_ptr<SearchProvider> getActiveProvider() const { return activeProvider; }

	// Provider persistence
	void loadProviderFromConfig(class ConfigManager &configManager);
	void saveProviderToConfig(class ConfigManager &configManager);

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

	// Provider management
	std::shared_ptr<SearchProvider> activeProvider;
	std::shared_ptr<TorrentsCsvProvider> torrentsCsvProvider;
	std::shared_ptr<BitsearchProvider> bitsearchProvider;
	std::shared_ptr<MultiProvider> multiProvider;
	std::string activeProviderName;

	std::vector<std::string> searchHistory;
	std::vector<TorrentSearchResult> favorites;
	std::atomic<uint64_t> favoritesRevision{0};
	std::unordered_set<std::string> favoriteHashes;
	mutable std::mutex favoritesMutex;

	// HTTP client methods (kept for backward compatibility)
	Result makeHttpRequest(const std::string &url, std::string &response);
	std::string buildSearchUrl(const SearchQuery &query) const;
	Result parseSearchResponse(const std::string &response, std::vector<TorrentSearchResult> &results);
	Result parseSearchResponse(const std::string &response, SearchResponse &searchResponse);

	// Utility methods
	void initializeProviders();
};
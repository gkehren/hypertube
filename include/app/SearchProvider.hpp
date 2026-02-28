#pragma once

#include "Result.hpp"
#include "SearchEngine.hpp"
#include <string>
#include <vector>
#include <memory>

// Forward declarations
struct TorrentSearchResult;
struct SearchQuery;
struct SearchResponse;

/**
 * @brief Abstract base class for torrent search providers
 *
 * This interface allows implementing different search providers
 * (e.g., torrents-csv, TPB, 1337x, etc.) with a common API
 */
class SearchProvider
{
public:
	virtual ~SearchProvider() = default;

	/**
	 * @brief Get the name of this provider
	 */
	virtual std::string getName() const = 0;

	/**
	 * @brief Get a description of this provider
	 */
	virtual std::string getDescription() const = 0;

	/**
	 * @brief Check if this provider is currently available
	 */
	virtual bool isAvailable() const = 0;

	/**
	 * @brief Search for torrents using this provider
	 * @param query The search query with parameters
	 * @param response The search response with results and pagination info
	 * @return Result indicating success or failure
	 */
	virtual Result search(const SearchQuery &query, SearchResponse &response) = 0;

	/**
	 * @brief Set the timeout for HTTP requests
	 * @param seconds Timeout in seconds
	 */
	virtual void setTimeout(int seconds) = 0;

	/**
	 * @brief Set the maximum number of retries for failed requests
	 * @param retries Number of retries
	 */
	virtual void setMaxRetries(int retries) = 0;

protected:
	/**
	 * @brief Helper to make HTTP requests
	 * @param url The URL to request
	 * @param response The response body
	 * @return Result indicating success or failure
	 */
	Result makeHttpRequest(const std::string &url, std::string &response, int timeoutSeconds, int maxRetries);
};

/**
 * @brief torrents-csv.com search provider
 */
class TorrentsCsvProvider : public SearchProvider
{
public:
	TorrentsCsvProvider();
	~TorrentsCsvProvider() override = default;

	std::string getName() const override { return "torrents-csv.com"; }
	std::string getDescription() const override { return "Large torrent database with good search"; }
	bool isAvailable() const override { return true; }

	Result search(const SearchQuery &query, SearchResponse &response) override;
	void setTimeout(int seconds) override { timeoutSeconds = seconds; }
	void setMaxRetries(int retries) override { maxRetries = retries; }

private:
	std::string apiUrl;
	int timeoutSeconds;
	int maxRetries;

	std::string buildSearchUrl(const SearchQuery &query) const;
	Result parseSearchResponse(const std::string &response, SearchResponse &searchResponse);
};

/**
 * @brief Bitsearch.to search provider
 */
class BitsearchProvider : public SearchProvider
{
public:
	BitsearchProvider();
	~BitsearchProvider() override = default;

	std::string getName() const override { return "Bitsearch.to"; }
	std::string getDescription() const override { return "Fast torrent search engine"; }
	bool isAvailable() const override { return true; }

	Result search(const SearchQuery &query, SearchResponse &response) override;
	void setTimeout(int seconds) override { timeoutSeconds = seconds; }
	void setMaxRetries(int retries) override { maxRetries = retries; }

private:
	std::string apiUrl;
	int timeoutSeconds;
	int maxRetries;

	std::string buildSearchUrl(const SearchQuery &query) const;
	Result parseSearchResponse(const std::string &response, SearchResponse &searchResponse);
};

/**
 * @brief Combined multi-provider that searches all available providers
 */
class MultiProvider : public SearchProvider
{
public:
	MultiProvider();
	~MultiProvider() override = default;

	std::string getName() const override { return "All Sources"; }
	std::string getDescription() const override { return "Search all available providers"; }
	bool isAvailable() const override { return true; }

	Result search(const SearchQuery &query, SearchResponse &response) override;
	void setTimeout(int seconds) override;
	void setMaxRetries(int retries) override;

	void addProvider(std::shared_ptr<SearchProvider> provider);
	const std::vector<std::shared_ptr<SearchProvider>> &getProviders() const { return providers; }

private:
	std::vector<std::shared_ptr<SearchProvider>> providers;
};

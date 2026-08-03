#include "SearchEngine.hpp"
#include "ConfigManager.hpp"
#include "utils/StringUtils.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <set>

using json = nlohmann::json;

// Callback function for cURL to write data with max size protection (10MB limit)
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s)
{
	size_t newLength = size * nmemb;
	static constexpr size_t MAX_RESPONSE_SIZE = 10 * 1024 * 1024; // 10 MB
	if (s->size() + newLength > MAX_RESPONSE_SIZE)
	{
		return 0; // Abort download if exceeding max size
	}
	try
	{
		s->append((char *)contents, newLength);
		return newLength;
	}
	catch (std::bad_alloc &e)
	{
		return 0;
	}
}

// Progress callback for cURL to support cancellation
static int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	SearchEngine *engine = static_cast<SearchEngine *>(clientp);
	// Return non-zero to abort the transfer if cancellation was requested
	if (engine && engine->isCancellationRequested())
	{
		return 1; // Abort transfer
	}
	return 0; // Continue transfer
}

SearchEngine::SearchEngine()
	: apiUrl("https://torrents-csv.com/service/search"),
	  timeoutSeconds(30),
	  maxRetries(3),
	  searching(false),
	  cancelRequested(false)
{
}

SearchEngine::~SearchEngine()
{
	// Cancel any ongoing search and wait for thread to finish
	cancelCurrentSearch();
	std::thread threadToJoin;
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		threadToJoin = std::move(searchThread);
	}
	if (threadToJoin.joinable())
	{
		threadToJoin.join();
	}
}

Result SearchEngine::searchTorrents(const SearchQuery &query, std::vector<TorrentSearchResult> &results)
{
	if (!tryStartSearch())
	{
		return Result::Failure("Search already in progress");
	}

	results.clear();

	SearchResponse response;
	Result httpResult = performSearch(query, response);
	finishSearch();

	if (!httpResult)
	{
		return httpResult;
	}

	if (httpResult)
	{
		addToSearchHistory(query.query);
	}
	results = std::move(response.torrents);
	return httpResult;
}

Result SearchEngine::searchTorrents(const SearchQuery &query, SearchResponse &response)
{
	if (!tryStartSearch())
	{
		return Result::Failure("Search already in progress");
	}

	response.torrents.clear();
	response.nextToken.clear();
	response.hasMore = false;

	Result httpResult = performSearch(query, response);
	finishSearch();

	if (httpResult)
		addToSearchHistory(query.query);
	return httpResult;
}

Result SearchEngine::registerSearchProvider(const std::string &id, SearchProvider provider)
{
	if (id.empty() || !provider)
		return Result::Failure("Search provider id and callback are required");
	std::lock_guard<std::mutex> lock(providersMutex);
	providers[id] = std::move(provider);
	return Result::Success();
}

Result SearchEngine::setActiveSearchProvider(const std::string &id)
{
	std::lock_guard<std::mutex> lock(providersMutex);
	if (id != "torrents-csv" && providers.find(id) == providers.end())
		return Result::Failure("Unknown search provider: " + id);
	activeProvider = id;
	return Result::Success();
}

std::string SearchEngine::getActiveSearchProvider() const
{
	std::lock_guard<std::mutex> lock(providersMutex);
	return activeProvider;
}

std::vector<std::string> SearchEngine::getSearchProviders() const
{
	std::lock_guard<std::mutex> lock(providersMutex);
	std::vector<std::string> result{"torrents-csv"};
	for (const auto &[id, provider] : providers)
		result.push_back(id);
	return result;
}

Result SearchEngine::performSearch(const SearchQuery &query, SearchResponse &response)
{
	try
	{
		SearchProvider provider;
		{
			std::lock_guard<std::mutex> lock(providersMutex);
			auto it = providers.find(activeProvider);
			if (it != providers.end())
				provider = it->second;
		}

		if (provider)
			return provider(query, response, [this] { return cancelRequested.load(); });

		std::string httpResponse;
		Result httpResult = makeHttpRequest(buildSearchUrl(query), httpResponse);
		if (!httpResult)
			return httpResult;
		if (cancelRequested.load())
			return Result::Failure("Search cancelled by user");
		return parseSearchResponse(httpResponse, response);
	}
	catch (const std::exception &e)
	{
		Utils::Logger::error("search", "Search provider failed: " + std::string(e.what()));
		return Result::Failure("Search failed: " + std::string(e.what()));
	}
	catch (...)
	{
		Utils::Logger::error("search", "Search provider failed with an unknown exception");
		return Result::Failure("Search failed with an unknown error");
	}
}

Result SearchEngine::makeHttpRequest(const std::string &url, std::string &response)
{
	int timeout = 30;
	int retries = 3;
	{
		std::lock_guard<std::mutex> lock(settingsMutex);
		timeout = timeoutSeconds;
		retries = maxRetries;
	}
	retries = std::max(retries, 1);
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		return Result::Failure("Failed to initialize cURL");
	}

	CURLcode res;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout));
	curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, 10L * 1024L * 1024L); // 10 MB limit
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hypertube/1.0");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

	// Enable progress callback for cancellation support
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	// Perform the request
	for (int attempt = 0; attempt < retries; ++attempt)
	{
		res = curl_easy_perform(curl);

		// Check if operation was cancelled
		if (res == CURLE_ABORTED_BY_CALLBACK)
		{
			curl_easy_cleanup(curl);
			return Result::Failure("Search cancelled by user");
		}

		if (res == CURLE_OK)
		{
			long response_code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

			if (response_code == 200)
			{
				curl_easy_cleanup(curl);
				return Result::Success();
			}
			else
			{
				curl_easy_cleanup(curl);
				return Result::Failure("HTTP Error: " + std::to_string(response_code));
			}
		}

		// If not the last attempt, wait a bit before retrying
		if (attempt < retries - 1)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (attempt + 1)));
		}
	}

	std::string error_msg = "cURL Error: " + std::string(curl_easy_strerror(res));
	curl_easy_cleanup(curl);
	return Result::Failure(error_msg);
}

std::string SearchEngine::buildSearchUrl(const SearchQuery &query) const
{
	std::string configuredApiUrl;
	{
		std::lock_guard<std::mutex> lock(settingsMutex);
		configuredApiUrl = apiUrl;
	}
	std::string url = configuredApiUrl + "?q=" + Utils::urlEncode(query.query);

	// Add size parameter only if explicitly set and not default
	if (query.maxResults > 0 && query.maxResults != 50)
	{
		url += "&size=" + std::to_string(query.maxResults);
	}

	// Add after parameter for pagination using next token
	if (!query.nextToken.empty())
	{
		url += "&after=" + Utils::urlEncode(query.nextToken);
	}

	return url;
}

Result SearchEngine::parseSearchResponse(const std::string &response, std::vector<TorrentSearchResult> &results)
{
	try
	{
		json j = json::parse(response);

		// Handle different possible response formats
		json torrentsArray;

		if (j.is_array())
		{
			// Direct array of torrents
			torrentsArray = j;
		}
		else if (j.contains("torrents") && j["torrents"].is_array())
		{
			// Object with torrents array (this matches the actual API response)
			torrentsArray = j["torrents"];
		}
		else if (j.contains("data") && j["data"].is_array())
		{
			// Object with data array
			torrentsArray = j["data"];
		}
		else
		{
			return Result::Failure("Invalid response format: no torrent data found");
		}

		for (const auto &item : torrentsArray)
		{
			TorrentSearchResult result;

			// Parse name field
			auto itName = item.find("name");
			if (itName != item.end() && itName->is_string())
			{
				result.name = itName->get<std::string>();
			}
			else
			{
				continue; // Skip if no valid name
			}

			// Parse infohash field
			auto itInfoHash = item.find("infohash");
			if (itInfoHash != item.end() && itInfoHash->is_string())
			{
				result.infoHash = itInfoHash->get<std::string>();
			}
			else
			{
				continue; // Skip if no valid infohash
			}

			// Generate magnet URI
			result.magnetUri = Utils::formatMagnetUri(result.infoHash, result.name);

			// Parse numeric fields safely
			auto itSizeBytes = item.find("size_bytes");
			if (itSizeBytes != item.end() && itSizeBytes->is_number())
			{
				result.sizeBytes = itSizeBytes->get<size_t>();
			}
			else
			{
				result.sizeBytes = 0;
			}

			auto itSeeders = item.find("seeders");
			if (itSeeders != item.end() && itSeeders->is_number())
			{
				result.seeders = itSeeders->get<int>();
			}
			else
			{
				result.seeders = 0;
			}

			auto itLeechers = item.find("leechers");
			if (itLeechers != item.end() && itLeechers->is_number())
			{
				result.leechers = itLeechers->get<int>();
			}
			else
			{
				result.leechers = 0;
			}

			// Handle created_unix as number and convert to string (legacy field)
			auto itCreatedUnix = item.find("created_unix");
			if (itCreatedUnix != item.end() && itCreatedUnix->is_number())
			{
				result.dateUploaded = std::to_string(itCreatedUnix->get<long long>());
				// Also populate the new createdUnix field
				result.createdUnix = itCreatedUnix->get<long long>();
			}
			else
			{
				result.dateUploaded = "";
				result.createdUnix = 0;
			}

			// Handle scraped_date field
			auto itScrapedDate = item.find("scraped_date");
			if (itScrapedDate != item.end() && itScrapedDate->is_number())
			{
				result.scrapedDate = itScrapedDate->get<long long>();
			}
			else
			{
				result.scrapedDate = 0;
			}

			// Handle completed field
			auto itCompleted = item.find("completed");
			if (itCompleted != item.end() && itCompleted->is_number())
			{
				result.completed = itCompleted->get<int>();
			}
			else
			{
				result.completed = 0;
			}

			// Category is often not present in torrents-csv, set default
			result.category = "General";

			results.push_back(result);
		}

		Utils::Logger::debug("search", "Parsed " + std::to_string(results.size()) + " search results");
		return Result::Success();
	}
	catch (const json::exception &e)
	{
		return Result::Failure("JSON Parse Error: " + std::string(e.what()));
	}
	catch (const std::exception &e)
	{
		return Result::Failure("Parse Error: " + std::string(e.what()));
	}
}

Result SearchEngine::parseSearchResponse(const std::string &response, SearchResponse &searchResponse)
{
	try
	{
		json j = json::parse(response);

		// Handle different possible response formats
		json torrentsArray;

		if (j.is_array())
		{
			// Direct array of torrents
			torrentsArray = j;
		}
		else if (j.contains("torrents") && j["torrents"].is_array())
		{
			// Object with torrents array (this matches the actual API response)
			torrentsArray = j["torrents"];
		}
		else if (j.contains("data") && j["data"].is_array())
		{
			// Object with data array
			torrentsArray = j["data"];
		}
		else
		{
			return Result::Failure("Invalid response format: no torrent data found");
		}

		// Parse the next token for pagination
		if (j.contains("next"))
		{
			if (j["next"].is_number())
			{
				searchResponse.nextToken = std::to_string(j["next"].get<long long>());
				searchResponse.hasMore = true;
			}
			else if (j["next"].is_string())
			{
				searchResponse.nextToken = j["next"].get<std::string>();
				searchResponse.hasMore = !searchResponse.nextToken.empty();
			}
		}

		// Track seen infohashes to prevent duplicates
		std::set<std::string> seenHashes;
		for (const auto &existing : searchResponse.torrents)
		{
			seenHashes.insert(existing.infoHash);
		}

		for (const auto &item : torrentsArray)
		{
			TorrentSearchResult result;

			// Parse name field
			auto itName = item.find("name");
			if (itName != item.end() && itName->is_string())
			{
				result.name = itName->get<std::string>();
			}
			else
			{
				continue; // Skip if no valid name
			}

			// Parse infohash field
			auto itInfoHash = item.find("infohash");
			if (itInfoHash != item.end() && itInfoHash->is_string())
			{
				result.infoHash = itInfoHash->get<std::string>();
			}
			else
			{
				continue; // Skip if no valid infohash
			}

			// Skip duplicates
			if (seenHashes.find(result.infoHash) != seenHashes.end())
			{
				continue;
			}
			seenHashes.insert(result.infoHash);

			// Generate magnet URI
			result.magnetUri = Utils::formatMagnetUri(result.infoHash, result.name);

			// Parse numeric fields safely
			auto itSizeBytes = item.find("size_bytes");
			if (itSizeBytes != item.end() && itSizeBytes->is_number())
			{
				result.sizeBytes = itSizeBytes->get<size_t>();
			}
			else
			{
				result.sizeBytes = 0;
			}

			auto itSeeders = item.find("seeders");
			if (itSeeders != item.end() && itSeeders->is_number())
			{
				result.seeders = itSeeders->get<int>();
			}
			else
			{
				result.seeders = 0;
			}

			auto itLeechers = item.find("leechers");
			if (itLeechers != item.end() && itLeechers->is_number())
			{
				result.leechers = itLeechers->get<int>();
			}
			else
			{
				result.leechers = 0;
			}

			// Handle created_unix as number and convert to string (legacy field)
			auto itCreatedUnix = item.find("created_unix");
			if (itCreatedUnix != item.end() && itCreatedUnix->is_number())
			{
				result.dateUploaded = std::to_string(itCreatedUnix->get<long long>());
				// Also populate the new createdUnix field
				result.createdUnix = itCreatedUnix->get<long long>();
			}
			else
			{
				result.dateUploaded = "";
				result.createdUnix = 0;
			}

			// Handle scraped_date field
			auto itScrapedDate = item.find("scraped_date");
			if (itScrapedDate != item.end() && itScrapedDate->is_number())
			{
				result.scrapedDate = itScrapedDate->get<long long>();
			}
			else
			{
				result.scrapedDate = 0;
			}

			// Handle completed field
			auto itCompleted = item.find("completed");
			if (itCompleted != item.end() && itCompleted->is_number())
			{
				result.completed = itCompleted->get<int>();
			}
			else
			{
				result.completed = 0;
			}

			// Category is often not present in torrents-csv, set default
			result.category = "General";

			searchResponse.torrents.push_back(result);
		}

		Utils::Logger::debug("search", "Parsed " + std::to_string(searchResponse.torrents.size()) + " search results");
		if (searchResponse.hasMore)
			Utils::Logger::debug("search", "More results available with next token");
		return Result::Success();
	}
	catch (const json::exception &e)
	{
		return Result::Failure("JSON Parse Error: " + std::string(e.what()));
	}
	catch (const std::exception &e)
	{
		return Result::Failure("Parse Error: " + std::string(e.what()));
	}
}

void SearchEngine::addToSearchHistory(const std::string &query)
{
	if (query.empty())
		return;
	std::lock_guard<std::mutex> lock(historyMutex);
	// Remove if already exists to move to front
	auto it = std::find(searchHistory.begin(), searchHistory.end(), query);
	if (it != searchHistory.end())
	{
		searchHistory.erase(it);
	}

	searchHistory.insert(searchHistory.begin(), query);

	// Keep only last 20 searches
	if (searchHistory.size() > 20)
	{
		searchHistory.resize(20);
	}
}

std::vector<std::string> SearchEngine::getSearchHistory() const
{
	std::lock_guard<std::mutex> lock(historyMutex);
	return searchHistory;
}

void SearchEngine::clearSearchHistory()
{
	std::lock_guard<std::mutex> lock(historyMutex);
	searchHistory.clear();
}

void SearchEngine::addToFavorites(const TorrentSearchResult &result)
{
	std::lock_guard<std::mutex> lock(favoritesMutex);

	// Check if already in favorites using the set (O(1))
	if (favoriteHashes.find(result.infoHash) == favoriteHashes.end())
	{
		favorites.push_back(result);
		favoriteHashes.insert(result.infoHash);
		favoritesRevision++;
	}
}

void SearchEngine::removeFromFavorites(const std::string &infoHash)
{
	std::lock_guard<std::mutex> lock(favoritesMutex);

	auto initialSize = favorites.size();
	favorites.erase(
		std::remove_if(favorites.begin(), favorites.end(),
					   [&infoHash](const TorrentSearchResult &fav)
					   {
						   return fav.infoHash == infoHash;
					   }),
		favorites.end());

	favoriteHashes.erase(infoHash);
	if (favorites.size() != initialSize)
	{
		favoritesRevision++;
	}
}

std::vector<TorrentSearchResult> SearchEngine::getFavorites() const
{
	std::lock_guard<std::mutex> lock(favoritesMutex);
	return favorites;
}

bool SearchEngine::isFavorite(const std::string &infoHash) const
{
	std::lock_guard<std::mutex> lock(favoritesMutex);
	return favoriteHashes.find(infoHash) != favoriteHashes.end();
}

void SearchEngine::saveFavoritesAndHistory(ConfigManager &configManager)
{
	std::vector<TorrentSearchResult> favoritesSnapshot;
	std::vector<std::string> historySnapshot;
	{
		std::scoped_lock lock(favoritesMutex, historyMutex);
		favoritesSnapshot = favorites;
		historySnapshot = searchHistory;
	}
	configManager.saveFavoritesAndHistory(favoritesSnapshot, historySnapshot);
}

void SearchEngine::loadFavoritesAndHistory(ConfigManager &configManager)
{
	std::vector<TorrentSearchResult> favoritesSnapshot;
	std::vector<std::string> historySnapshot;
	configManager.loadFavoritesAndHistory(favoritesSnapshot, historySnapshot);
	std::scoped_lock lock(favoritesMutex, historyMutex);
	favorites = std::move(favoritesSnapshot);
	searchHistory = std::move(historySnapshot);

	favoriteHashes.clear();
	for (const auto &fav : favorites)
	{
		favoriteHashes.insert(fav.infoHash);
	}
	favoritesRevision++;
}

void SearchEngine::setApiUrl(const std::string &url)
{
	std::lock_guard<std::mutex> lock(settingsMutex);
	apiUrl = url;
}

void SearchEngine::setTimeout(int seconds)
{
	std::lock_guard<std::mutex> lock(settingsMutex);
	timeoutSeconds = std::max(seconds, 1);
}

void SearchEngine::setMaxRetries(int retries)
{
	std::lock_guard<std::mutex> lock(settingsMutex);
	maxRetries = std::max(retries, 1);
}

bool SearchEngine::isSearching() const
{
	return searching;
}

void SearchEngine::cancelCurrentSearch()
{
	if (searching.load())
		cancelRequested = true;
}

bool SearchEngine::tryStartSearch()
{
	std::lock_guard<std::mutex> lock(searchMutex);
	if (searching.load())
		return false;
	searching = true;
	cancelRequested = false;
	return true;
}

void SearchEngine::finishSearch()
{
	searching = false;
}

// Async search implementation with threading
void SearchEngine::searchTorrentsAsync(const SearchQuery &query, std::function<void(Result, SearchResponse)> callback)
{
	// If already searching, ignore this request
	if (searching.load())
		return;

	// Join previous thread if it exists
	std::thread previousThread;
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		previousThread = std::move(searchThread);
	}
	if (previousThread.joinable())
	{
		previousThread.join();
	}

	// Reset cancellation flag
	if (!tryStartSearch())
		return;

	// Launch search in separate thread
	std::thread worker([this, query, callback]()
	{
		SearchResponse response;
		Result result = Result::Failure("Unknown error");

		try
		{
			if (cancelRequested.load())
			{
				result = Result::Failure("Search cancelled");
			}
			else
			{
				Result httpResult = performSearch(query, response);
				if (cancelRequested.load())
					result = Result::Failure("Search cancelled");
				else
				{
					result = httpResult;
					if (result)
						addToSearchHistory(query.query);
				}
			}
		}
		catch (const std::exception &e)
		{
			result = Result::Failure("Search failed: " + std::string(e.what()));
			Utils::Logger::error("search", result.message);
		}
		catch (...)
		{
			result = Result::Failure("Search failed with an unknown error");
			Utils::Logger::error("search", result.message);
		}

		finishSearch();
		if (callback)
		{
			try
			{
				callback(result, response);
			}
			catch (const std::exception &e)
			{
				Utils::Logger::error("search", "Search callback failed: " + std::string(e.what()));
			}
			catch (...)
			{
				Utils::Logger::error("search", "Search callback failed with an unknown exception");
			}
		}
	});
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		searchThread = std::move(worker);
	}
}

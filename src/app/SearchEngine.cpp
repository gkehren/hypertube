#include "SearchEngine.hpp"
#include "ConfigManager.hpp"
#include <curl/curl.h>
#include <json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <set>

using json = nlohmann::json;

// Callback function for cURL to write data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s)
{
	size_t newLength = size * nmemb;
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
	// Initialize cURL globally
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

SearchEngine::~SearchEngine()
{
	// Cancel any ongoing search and wait for thread to finish
	cancelCurrentSearch();
	if (searchThread.joinable())
	{
		searchThread.join();
	}
	curl_global_cleanup();
}

Result SearchEngine::searchTorrents(const SearchQuery &query, std::vector<TorrentSearchResult> &results)
{
	if (searching)
	{
		return Result::Failure("Search already in progress");
	}

	searching = true;
	results.clear();

	std::string url = buildSearchUrl(query);
	std::cout << "Searching with URL: " << url << std::endl;
	std::string response;

	Result httpResult = makeHttpRequest(url, response);
	searching = false;

	if (!httpResult)
	{
		return httpResult;
	}

	Result parseResult = parseSearchResponse(response, results);
	if (parseResult)
	{
		addToSearchHistory(query.query);
	}

	return parseResult;
}

Result SearchEngine::searchTorrents(const SearchQuery &query, SearchResponse &response)
{
	if (searching)
	{
		return Result::Failure("Search already in progress");
	}

	searching = true;
	response.torrents.clear();
	response.nextToken.clear();
	response.hasMore = false;

	std::string url = buildSearchUrl(query);
	std::cout << "Searching with URL: " << url << std::endl;
	std::string httpResponse;

	Result httpResult = makeHttpRequest(url, httpResponse);
	searching = false;

	if (!httpResult)
	{
		return httpResult;
	}

	Result parseResult = parseSearchResponse(httpResponse, response);
	if (parseResult)
	{
		addToSearchHistory(query.query);
	}

	return parseResult;
}

Result SearchEngine::makeHttpRequest(const std::string &url, std::string &response)
{
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		return Result::Failure("Failed to initialize cURL");
	}

	CURLcode res;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeoutSeconds);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hypertube/1.0");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

	// Enable progress callback for cancellation support
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	// Perform the request
	for (int attempt = 0; attempt < maxRetries; ++attempt)
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
		if (attempt < maxRetries - 1)
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
	std::string url = apiUrl + "?q=" + urlEncode(query.query);

	// Add size parameter only if explicitly set and not default
	if (query.maxResults > 0 && query.maxResults != 50)
	{
		url += "&size=" + std::to_string(query.maxResults);
	}

	// Add after parameter for pagination using next token
	if (!query.nextToken.empty())
	{
		url += "&after=" + query.nextToken;
	}

	return url;
}

Result SearchEngine::parseSearchResponse(const std::string &response, std::vector<TorrentSearchResult> &results)
{
	try
	{
		std::cout << "Raw API response: " << response.substr(0, 500) << "..." << std::endl;
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
			result.magnetUri = formatMagnetUri(result.infoHash, result.name);

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

		std::cout << "Parsed " << results.size() << " search results" << std::endl;
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
		std::cout << "Raw API response: " << response.substr(0, 500) << "..." << std::endl;
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
			result.magnetUri = formatMagnetUri(result.infoHash, result.name);

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

		std::cout << "Parsed " << searchResponse.torrents.size() << " search results" << std::endl;
		if (searchResponse.hasMore)
		{
			std::cout << "More results available with next token: " << searchResponse.nextToken << std::endl;
		}
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

std::string SearchEngine::urlEncode(const std::string &value) const
{
	static const char lookup[] = "0123456789ABCDEF";
	std::string escaped;
	escaped.reserve(value.length() * 3);

	for (char c : value)
	{
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
		{
			escaped += c;
		}
		else
		{
			escaped += '%';
			escaped += lookup[(c >> 4) & 0x0F];
			escaped += lookup[c & 0x0F];
		}
	}

	return escaped;
}

std::string SearchEngine::formatMagnetUri(const std::string &infoHash, const std::string &name) const
{
	std::string magnet = "magnet:?xt=urn:btih:" + infoHash;
	if (!name.empty())
	{
		magnet += "&dn=" + urlEncode(name);
	}
	// Add some popular trackers
	magnet += "&tr=udp://tracker.openbittorrent.com:80"
			  "&tr=udp://tracker.opentrackr.org:1337"
			  "&tr=udp://tracker.coppersurfer.tk:6969";
	return magnet;
}

void SearchEngine::addToSearchHistory(const std::string &query)
{
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

const std::vector<std::string> &SearchEngine::getSearchHistory() const
{
	return searchHistory;
}

void SearchEngine::clearSearchHistory()
{
	searchHistory.clear();
}

void SearchEngine::addToFavorites(const TorrentSearchResult &result)
{
	// Check if already in favorites
	auto it = std::find_if(favorites.begin(), favorites.end(),
						   [&result](const TorrentSearchResult &fav)
						   {
							   return fav.infoHash == result.infoHash;
						   });

	if (it == favorites.end())
	{
		favorites.push_back(result);
	}
}

void SearchEngine::removeFromFavorites(const std::string &infoHash)
{
	favorites.erase(
		std::remove_if(favorites.begin(), favorites.end(),
					   [&infoHash](const TorrentSearchResult &fav)
					   {
						   return fav.infoHash == infoHash;
					   }),
		favorites.end());
}

const std::vector<TorrentSearchResult> &SearchEngine::getFavorites() const
{
	return favorites;
}

void SearchEngine::saveFavoritesAndHistory(ConfigManager &configManager)
{
	configManager.saveFavoritesAndHistory(favorites, searchHistory);
}

void SearchEngine::loadFavoritesAndHistory(ConfigManager &configManager)
{
	configManager.loadFavoritesAndHistory(favorites, searchHistory);
}

void SearchEngine::setApiUrl(const std::string &url)
{
	apiUrl = url;
}

void SearchEngine::setTimeout(int seconds)
{
	timeoutSeconds = seconds;
}

void SearchEngine::setMaxRetries(int retries)
{
	maxRetries = retries;
}

bool SearchEngine::isSearching() const
{
	return searching;
}

void SearchEngine::cancelCurrentSearch()
{
	cancelRequested = true;
}

// Async search implementation (simplified - would need proper threading in production)
Result SearchEngine::searchTorrentsAsync(const SearchQuery &query, std::function<void(const std::vector<TorrentSearchResult> &)> callback)
{
	// For now, just do synchronous search
	// In a full implementation, this would use std::async or threading
	std::vector<TorrentSearchResult> results;
	Result searchResult = searchTorrents(query, results);

	if (searchResult)
	{
		callback(results);
	}

	return searchResult;
}

// New threaded async search implementation
void SearchEngine::searchTorrentsAsyncThreaded(const SearchQuery &query, std::function<void(Result, SearchResponse)> callback)
{
	// If already searching, ignore this request
	if (searching.load())
	{
		return;
	}

	// Join previous thread if it exists
	if (searchThread.joinable())
	{
		searchThread.join();
	}

	// Reset cancellation flag
	cancelRequested = false;

	// Launch search in separate thread
	searchThread = std::thread([this, query, callback]()
							   {
		searching = true;
		SearchResponse response;
		response.torrents.clear();
		response.nextToken.clear();
		response.hasMore = false;

		Result result = Result::Failure("Unknown error");

		// Check cancellation before starting
		if (cancelRequested.load())
		{
			result = Result::Failure("Search cancelled");
		}
		else
		{
			// Perform the search directly (bypass the searching flag check)
			std::string url = buildSearchUrl(query);
			std::cout << "Searching with URL: " << url << std::endl;
			std::string httpResponse;

			Result httpResult = makeHttpRequest(url, httpResponse);

			// Check cancellation after HTTP request
			if (cancelRequested.load())
			{
				result = Result::Failure("Search cancelled");
			}
			else if (httpResult)
			{
				result = parseSearchResponse(httpResponse, response);
				if (result)
				{
					addToSearchHistory(query.query);
				}
			}
			else
			{
				result = httpResult;
			}
		}

		searching = false;

		// Always call the callback to reset UI state, even if cancelled
		callback(result, response); });
}
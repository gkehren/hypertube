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
#include <cctype>
#include <cstring>
#include <climits>

namespace
{
bool normalizeInfoHash(std::string &hash)
{
	if (hash.size() != 40 && hash.size() != 64)
		return false;
	for (char &character : hash)
	{
		if (!std::isxdigit(static_cast<unsigned char>(character)))
			return false;
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	return true;
}

std::string decodeXml(std::string value)
{
	const std::pair<const char *, const char *> entities[] = {
		{"&amp;", "&"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}};
	for (const auto &[encoded, decoded] : entities)
	{
		std::size_t position = 0;
		while ((position = value.find(encoded, position)) != std::string::npos)
		{
			value.replace(position, std::strlen(encoded), decoded);
			position += std::strlen(decoded);
		}
	}
	return value;
}

std::string xmlTagValue(const std::string &item, const std::string &tag)
{
	const std::string opening = "<" + tag;
	const auto openingPosition = item.find(opening);
	if (openingPosition == std::string::npos)
		return {};
	const auto valuePosition = item.find('>', openingPosition + opening.size());
	if (valuePosition == std::string::npos)
		return {};
	const auto closingPosition = item.find("</" + tag + ">", valuePosition + 1);
	if (closingPosition == std::string::npos)
		return {};
	return decodeXml(item.substr(valuePosition + 1, closingPosition - valuePosition - 1));
}

std::string xmlAttribute(const std::string &element, const std::string &attribute)
{
	const std::string marker = attribute + "=\"";
	const auto start = element.find(marker);
	if (start == std::string::npos)
		return {};
	const auto valueStart = start + marker.size();
	const auto end = element.find('"', valueStart);
	return end == std::string::npos ? std::string{} : decodeXml(element.substr(valueStart, end - valueStart));
}

std::string torznabAttribute(const std::string &item, const std::string &name)
{
	std::size_t position = 0;
	while ((position = item.find("<torznab:attr", position)) != std::string::npos)
	{
		const auto end = item.find('>', position);
		if (end == std::string::npos)
			return {};
		const std::string element = item.substr(position, end - position + 1);
		if (xmlAttribute(element, "name") == name)
			return xmlAttribute(element, "value");
		position = end + 1;
	}
	return {};
}

long long parseNonNegative(const std::string &value)
{
	if (value.empty())
		return 0;
	try
	{
		std::size_t consumed = 0;
		const long long parsed = std::stoll(value, &consumed);
		return consumed == value.size() && parsed >= 0 ? parsed : 0;
	}
	catch (...)
	{
		return 0;
	}
}

uint64_t jsonNonNegativeInteger(const nlohmann::json &value, uint64_t maximum)
{
	try
	{
		uint64_t parsed = 0;
		if (value.is_number_unsigned())
			parsed = value.get<uint64_t>();
		else if (value.is_number_integer())
		{
			const int64_t signedValue = value.get<int64_t>();
			if (signedValue < 0)
				return 0;
			parsed = static_cast<uint64_t>(signedValue);
		}
		else
			return 0;
		return std::min(parsed, maximum);
	}
	catch (...)
	{
		return 0;
	}
}
}

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
	workerThread_ = std::thread(&SearchEngine::workerLoop, this);
}

SearchEngine::~SearchEngine()
{
	shutdown();
}

void SearchEngine::shutdown()
{
	shuttingDown = true;
	cancelRequested = true;
	{
		std::lock_guard<std::mutex> lock(queueMutex_);
		stopWorker_ = true;
	}
	queueCv_.notify_all();
	if (workerThread_.joinable())
	{
		workerThread_.join();
	}
	cleanupCurlHandle();
}

void SearchEngine::cleanupCurlHandle()
{
	std::lock_guard<std::mutex> lock(curlMutex_);
	if (curlHandle_)
	{
		curl_easy_cleanup(static_cast<CURL *>(curlHandle_));
		curlHandle_ = nullptr;
	}
}

void SearchEngine::workerLoop()
{
	while (true)
	{
		SearchTask task;
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			queueCv_.wait(lock, [this] { return stopWorker_ || hasWork_; });
			if (!hasWork_ && stopWorker_)
				break;
			task = std::move(pendingTask_);
			hasWork_ = false;
		}

		SearchResponse response;
		Result result = Result::Failure("Unknown error");

		try
		{
			if (cancelRequested.load())
			{
				result = Result::Failure("Search cancelled", ResultCode::Cancelled);
			}
			else
			{
				Result httpResult = performSearch(task.query, response);
				if (cancelRequested.load())
					result = Result::Failure("Search cancelled", ResultCode::Cancelled);
				else
				{
					result = httpResult;
					if (result)
						addToSearchHistory(task.query.query);
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

		{
			std::lock_guard<std::mutex> lock(completionMutex);
			completedSearch = CompletedSearch{task.requestId, std::move(result), std::move(response)};
		}
		finishSearch();
	}
}

Result SearchEngine::searchTorrents(const SearchQuery &query, std::vector<TorrentSearchResult> &results)
{
	if (!tryStartSearch())
	{
		return Result::Failure("Search already in progress", ResultCode::Busy, true);
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
		return Result::Failure("Search already in progress", ResultCode::Busy, true);
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
		return Result::Failure("Search provider id and callback are required", ResultCode::InvalidInput);
	std::lock_guard<std::mutex> lock(providersMutex);
	providers[id] = std::move(provider);
	return Result::Success();
}

Result SearchEngine::setActiveSearchProvider(const std::string &id)
{
	{
		std::lock_guard<std::mutex> lock(providersMutex);
		if (id != "torrents-csv" && providers.find(id) == providers.end())
			return Result::Failure("Unknown search provider: " + id, ResultCode::NotFound);
		activeProvider = id;
	}
	clearSearchCache();
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

Result SearchEngine::configureTorznabProvider(const std::string &url, const std::string &apiKey)
{
	Result validation = validateTorznabConfig(url);
	if (!validation)
		return validation;
	std::string endpoint = url;
	while (!endpoint.empty() && endpoint.back() == '/')
		endpoint.pop_back();
	return registerSearchProvider("torznab", [this, endpoint, apiKey](const SearchQuery &query, SearchResponse &response, const std::function<bool()> &cancelled)
	{
		if (cancelled())
			return Result::Failure("Search cancelled", ResultCode::Cancelled);
		std::string requestUrl = endpoint + (endpoint.find('?') == std::string::npos ? "?" : "&");
		requestUrl += "t=search&q=" + Utils::urlEncode(query.query);
		if (query.maxResults > 0)
			requestUrl += "&limit=" + std::to_string(query.maxResults);
		if (!query.nextToken.empty())
			requestUrl += "&offset=" + Utils::urlEncode(query.nextToken);
		if (!apiKey.empty())
			requestUrl += "&apikey=" + Utils::urlEncode(apiKey);
		std::string body;
		Result request = makeHttpRequest(requestUrl, body);
		if (!request)
			return request;
		return parseTorznabResponse(body, response);
	});
}

Result SearchEngine::validateTorznabConfig(const std::string &url)
{
	if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
		return Result::Failure("Torznab URL must use http:// or https://", ResultCode::InvalidInput);
	if (url.size() <= url.find("://") + 3)
		return Result::Failure("Torznab URL must include a host", ResultCode::InvalidInput);
	return Result::Success();
}

void SearchEngine::clearSearchCache()
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	searchCache.clear();
}

Result SearchEngine::performSearch(const SearchQuery &query, SearchResponse &response)
{
	try
	{
		SearchProvider provider;
		std::string providerId;
		{
			std::lock_guard<std::mutex> lock(providersMutex);
			providerId = activeProvider;
			auto it = providers.find(activeProvider);
			if (it != providers.end())
				provider = it->second;
		}
		const std::string cacheKey = providerId + "\n" + query.query + "\n" + std::to_string(query.maxResults) + "\n" + query.nextToken;
		{
			std::lock_guard<std::mutex> lock(cacheMutex);
			auto cached = searchCache.find(cacheKey);
			if (cached != searchCache.end())
			{
				if (cached->second.expiresAt > std::chrono::steady_clock::now())
				{
					response = cached->second.response;
					return Result::Success();
				}
				searchCache.erase(cached);
			}
		}

		if (provider)
		{
			Result providerResult = provider(query, response, [this] { return cancelRequested.load(); });
			if (providerResult)
			{
				std::lock_guard<std::mutex> lock(cacheMutex);
				if (searchCache.size() >= 100)
					searchCache.clear();
				searchCache[cacheKey] = CachedSearch{response, std::chrono::steady_clock::now() + std::chrono::minutes(5)};
				return providerResult;
			}
			if (providerResult.code == ResultCode::Cancelled || !query.nextToken.empty())
				return providerResult;
			Utils::Logger::warning("search", "Active provider failed; falling back to torrents-csv: " + providerResult.message);
			response = SearchResponse{};
		}

		std::string httpResponse;
		Result httpResult = makeHttpRequest(buildSearchUrl(query), httpResponse);
		if (!httpResult)
			return httpResult;
		if (cancelRequested.load())
			return Result::Failure("Search cancelled by user", ResultCode::Cancelled);
		Result parseResult = parseSearchResponse(httpResponse, response);
		if (parseResult)
		{
			std::lock_guard<std::mutex> lock(cacheMutex);
			if (searchCache.size() >= 100)
				searchCache.clear();
			searchCache[cacheKey] = CachedSearch{response, std::chrono::steady_clock::now() + std::chrono::minutes(5)};
		}
		return parseResult;
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
	bool useProxy = false;
	std::string configuredProxyType;
	std::string configuredProxyHost;
	std::string configuredProxyUsername;
	std::string configuredProxyPassword;
	int configuredProxyPort = 0;
	{
		std::lock_guard<std::mutex> lock(settingsMutex);
		timeout = timeoutSeconds;
		retries = maxRetries;
		useProxy = proxyEnabled;
		configuredProxyType = proxyType;
		configuredProxyHost = proxyHost;
		configuredProxyPort = proxyPort;
		configuredProxyUsername = proxyUsername;
		configuredProxyPassword = proxyPassword;
	}
	retries = std::max(retries, 1);

	std::lock_guard<std::mutex> lock(curlMutex_);
	CURL *curl = static_cast<CURL *>(curlHandle_);
	if (!curl)
	{
		curl = curl_easy_init();
		if (!curl)
			return Result::Failure("Failed to initialize cURL");
		curlHandle_ = curl;
	}
	else
	{
		curl_easy_reset(curl);
	}

	CURLcode res = CURLE_OK;
	long lastResponseCode = 0;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout));
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(std::min(timeout, 10)));
	curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, 10L * 1024L * 1024L); // 10 MB limit
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hypertube/1.0");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
	curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
	if (useProxy)
	{
		curl_easy_setopt(curl, CURLOPT_PROXY, configuredProxyHost.c_str());
		curl_easy_setopt(curl, CURLOPT_PROXYPORT, static_cast<long>(configuredProxyPort));
		curl_easy_setopt(curl, CURLOPT_PROXYTYPE,
			configuredProxyType == "http" ? CURLPROXY_HTTP : CURLPROXY_SOCKS5_HOSTNAME);
		if (!configuredProxyUsername.empty())
		{
			curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, configuredProxyUsername.c_str());
			curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, configuredProxyPassword.c_str());
		}
	}

	// Enable progress callback for cancellation support
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	// Perform the request
	for (int attempt = 0; attempt < retries; ++attempt)
	{
		response.clear();
		res = curl_easy_perform(curl);

		// Check if operation was cancelled
		if (res == CURLE_ABORTED_BY_CALLBACK)
		{
			return Result::Failure("Search cancelled by user", ResultCode::Cancelled);
		}

		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &lastResponseCode);

			if (lastResponseCode == 200)
			{
				return Result::Success();
			}
			if (lastResponseCode == 401 || lastResponseCode == 403)
			{
				return Result::Failure("HTTP Error: " + std::to_string(lastResponseCode), ResultCode::Unauthorized);
			}
			const bool transientHttpError = lastResponseCode == 429 || lastResponseCode >= 500;
			if (!transientHttpError || attempt == retries - 1)
			{
				if (lastResponseCode == 429)
					return Result::Failure("HTTP Error: 429", ResultCode::RateLimited, true);
				return Result::Failure("HTTP Error: " + std::to_string(lastResponseCode), ResultCode::Network, transientHttpError);
			}
		}

		// If not the last attempt, wait a bit before retrying
		if (attempt < retries - 1)
		{
			const auto deadline = std::chrono::steady_clock::now()
				+ std::chrono::milliseconds(500 * (1 << std::min(attempt, 4)));
			while (std::chrono::steady_clock::now() < deadline)
			{
				if (cancelRequested.load())
				{
					return Result::Failure("Search cancelled by user", ResultCode::Cancelled);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
			}
		}
	}

	std::string error_msg = lastResponseCode != 0
		? "HTTP Error: " + std::to_string(lastResponseCode)
		: "cURL Error: " + std::string(curl_easy_strerror(res));
	return Result::Failure(error_msg, ResultCode::Network, true);
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
				if (!normalizeInfoHash(result.infoHash))
					continue;
			}
			else
			{
				continue; // Skip if no valid infohash
			}

			// Generate magnet URI
			result.magnetUri = Utils::formatMagnetUri(result.infoHash, result.name);

			// Parse numeric fields safely
			auto itSizeBytes = item.find("size_bytes");
			if (itSizeBytes != item.end())
				result.sizeBytes = static_cast<size_t>(jsonNonNegativeInteger(*itSizeBytes, SIZE_MAX));

			auto itSeeders = item.find("seeders");
			if (itSeeders != item.end())
				result.seeders = static_cast<int>(jsonNonNegativeInteger(*itSeeders, INT_MAX));

			auto itLeechers = item.find("leechers");
			if (itLeechers != item.end())
				result.leechers = static_cast<int>(jsonNonNegativeInteger(*itLeechers, INT_MAX));

			// Handle created_unix as number and convert to string (legacy field)
			auto itCreatedUnix = item.find("created_unix");
			if (itCreatedUnix != item.end())
			{
				result.createdUnix = static_cast<int64_t>(jsonNonNegativeInteger(*itCreatedUnix, INT64_MAX));
				result.dateUploaded = result.createdUnix == 0 ? "" : std::to_string(result.createdUnix);
			}
			else
			{
				result.dateUploaded = "";
				result.createdUnix = 0;
			}

			// Handle scraped_date field
			auto itScrapedDate = item.find("scraped_date");
			if (itScrapedDate != item.end())
				result.scrapedDate = static_cast<int64_t>(jsonNonNegativeInteger(*itScrapedDate, INT64_MAX));

			// Handle completed field
			auto itCompleted = item.find("completed");
			if (itCompleted != item.end())
				result.completed = static_cast<int>(jsonNonNegativeInteger(*itCompleted, INT_MAX));

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
				if (!normalizeInfoHash(result.infoHash))
					continue;
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
			if (itSizeBytes != item.end())
				result.sizeBytes = static_cast<size_t>(jsonNonNegativeInteger(*itSizeBytes, SIZE_MAX));

			auto itSeeders = item.find("seeders");
			if (itSeeders != item.end())
				result.seeders = static_cast<int>(jsonNonNegativeInteger(*itSeeders, INT_MAX));

			auto itLeechers = item.find("leechers");
			if (itLeechers != item.end())
				result.leechers = static_cast<int>(jsonNonNegativeInteger(*itLeechers, INT_MAX));

			// Handle created_unix as number and convert to string (legacy field)
			auto itCreatedUnix = item.find("created_unix");
			if (itCreatedUnix != item.end())
			{
				result.createdUnix = static_cast<int64_t>(jsonNonNegativeInteger(*itCreatedUnix, INT64_MAX));
				result.dateUploaded = result.createdUnix == 0 ? "" : std::to_string(result.createdUnix);
			}
			else
			{
				result.dateUploaded = "";
				result.createdUnix = 0;
			}

			// Handle scraped_date field
			auto itScrapedDate = item.find("scraped_date");
			if (itScrapedDate != item.end())
				result.scrapedDate = static_cast<int64_t>(jsonNonNegativeInteger(*itScrapedDate, INT64_MAX));

			// Handle completed field
			auto itCompleted = item.find("completed");
			if (itCompleted != item.end())
				result.completed = static_cast<int>(jsonNonNegativeInteger(*itCompleted, INT_MAX));

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

Result SearchEngine::parseTorznabResponse(const std::string &response, SearchResponse &searchResponse)
{
	searchResponse = SearchResponse{};
	if (response.empty())
		return Result::Failure("Torznab returned an empty response", ResultCode::Parse);
	if (response.find("<error") != std::string::npos)
	{
		const auto errorPosition = response.find("<error");
		const auto errorEnd = response.find('>', errorPosition);
		const std::string element = errorEnd == std::string::npos ? std::string{} : response.substr(errorPosition, errorEnd - errorPosition + 1);
		const std::string description = xmlAttribute(element, "description");
		return Result::Failure(description.empty() ? "Torznab provider returned an error" : description, ResultCode::Unavailable);
	}

	std::unordered_set<std::string> seen;
	std::size_t position = 0;
	while (searchResponse.torrents.size() < 500 && (position = response.find("<item", position)) != std::string::npos)
	{
		const auto itemStart = response.find('>', position);
		const auto itemEnd = response.find("</item>", itemStart);
		if (itemStart == std::string::npos || itemEnd == std::string::npos)
			return Result::Failure("Malformed Torznab item", ResultCode::Parse);
		const std::string item = response.substr(itemStart + 1, itemEnd - itemStart - 1);
		position = itemEnd + 7;

		TorrentSearchResult result;
		result.name = xmlTagValue(item, "title");
		result.infoHash = torznabAttribute(item, "infohash");
		result.magnetUri = torznabAttribute(item, "magneturl");
		if (result.magnetUri.empty())
		{
			const std::string link = xmlTagValue(item, "link");
			if (link.rfind("magnet:?", 0) == 0)
				result.magnetUri = link;
		}
		if (result.infoHash.empty() && !result.magnetUri.empty())
		{
			const auto hashPosition = result.magnetUri.find("btih:");
			if (hashPosition != std::string::npos && hashPosition + 45 <= result.magnetUri.size())
				result.infoHash = result.magnetUri.substr(hashPosition + 5, 40);
		}
		if (result.infoHash.empty())
			result.infoHash = xmlTagValue(item, "guid");
		if (result.name.empty() || !normalizeInfoHash(result.infoHash) || !seen.insert(result.infoHash).second)
			continue;
		if (result.magnetUri.empty())
			result.magnetUri = Utils::formatMagnetUri(result.infoHash, result.name);

		result.sizeBytes = static_cast<std::size_t>(parseNonNegative(xmlTagValue(item, "size")));
		result.seeders = static_cast<int>(std::min<long long>(parseNonNegative(torznabAttribute(item, "seeders")), INT_MAX));
		result.leechers = static_cast<int>(std::min<long long>(parseNonNegative(torznabAttribute(item, "peers")), INT_MAX));
		result.category = xmlTagValue(item, "category");
		if (result.category.empty())
			result.category = "General";
		result.dateUploaded = xmlTagValue(item, "pubDate");
		searchResponse.torrents.push_back(std::move(result));
	}

	const auto responsePosition = response.find("newznab:response");
	if (responsePosition != std::string::npos)
	{
		const auto responseEnd = response.find('>', responsePosition);
		const std::string element = responseEnd == std::string::npos ? std::string{} : response.substr(responsePosition, responseEnd - responsePosition + 1);
		const long long offset = parseNonNegative(xmlAttribute(element, "offset"));
		const long long total = parseNonNegative(xmlAttribute(element, "total"));
		const long long next = offset + static_cast<long long>(searchResponse.torrents.size());
		searchResponse.hasMore = next < total;
		if (searchResponse.hasMore)
			searchResponse.nextToken = std::to_string(next);
	}
	return Result::Success();
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
	{
		std::lock_guard<std::mutex> lock(settingsMutex);
		apiUrl = url;
	}
	clearSearchCache();
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

Result SearchEngine::setProxyConfig(bool enabled, const std::string &type, const std::string &host,
	int port, const std::string &username, const std::string &password)
{
	Result validation = validateProxyConfig(enabled, type, host, port);
	if (!validation)
		return validation;
	{
		std::lock_guard<std::mutex> lock(settingsMutex);
		proxyEnabled = enabled;
		proxyType = type;
		proxyHost = host;
		proxyPort = port;
		proxyUsername = username;
		proxyPassword = password;
	}
	clearSearchCache();
	return Result::Success();
}

Result SearchEngine::validateProxyConfig(bool enabled, const std::string &type, const std::string &host, int port)
{
	if (enabled && host.empty())
		return Result::Failure("Proxy host cannot be empty", ResultCode::InvalidInput);
	if (enabled && (port < 1 || port > 65535))
		return Result::Failure("Proxy port must be between 1 and 65535", ResultCode::InvalidInput);
	if (type != "socks5" && type != "http")
		return Result::Failure("Proxy type must be socks5 or http", ResultCode::InvalidInput);
	return Result::Success();
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
	std::lock_guard<std::mutex> lock(queueMutex_);
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

Result SearchEngine::startSearch(const SearchQuery &query, uint64_t &requestId)
{
	if (query.query.empty())
		return Result::Failure("Search query cannot be empty", ResultCode::InvalidInput);
	if (shuttingDown.load())
		return Result::Failure("Search service is shutting down", ResultCode::Unavailable);
	if (!tryStartSearch())
		return Result::Failure("Search already in progress", ResultCode::Busy);

	requestId = nextRequestId.fetch_add(1);
	{
		std::lock_guard<std::mutex> lock(queueMutex_);
		pendingTask_ = SearchTask{query, requestId};
		hasWork_ = true;
	}
	queueCv_.notify_one();
	return Result::Success();
}

std::optional<CompletedSearch> SearchEngine::takeCompletedSearch()
{
	std::lock_guard<std::mutex> lock(completionMutex);
	if (!completedSearch)
		return std::nullopt;
	auto completion = std::move(completedSearch);
	completedSearch.reset();
	return completion;
}

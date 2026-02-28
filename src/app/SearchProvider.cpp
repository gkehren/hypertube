#include "SearchProvider.hpp"
#include "utils/StringUtils.hpp"
#include <curl/curl.h>
#include <json.hpp>
#include <iostream>
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

// ============================================================================
// SearchProvider base class implementation
// ============================================================================

Result SearchProvider::makeHttpRequest(const std::string &url, std::string &response, int timeoutSeconds, int maxRetries)
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

	// Perform the request with retries
	for (int attempt = 0; attempt < maxRetries; ++attempt)
	{
		res = curl_easy_perform(curl);

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

		// If not the last attempt, wait before retrying
		if (attempt < maxRetries - 1)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (attempt + 1)));
		}
	}

	std::string error_msg = "cURL Error: " + std::string(curl_easy_strerror(res));
	curl_easy_cleanup(curl);
	return Result::Failure(error_msg);
}

// ============================================================================
// TorrentsCsvProvider implementation
// ============================================================================

TorrentsCsvProvider::TorrentsCsvProvider()
	: apiUrl("https://torrents-csv.com/service/search"),
	  timeoutSeconds(30),
	  maxRetries(3)
{
}

std::string TorrentsCsvProvider::buildSearchUrl(const SearchQuery &query) const
{
	std::string url = apiUrl + "?q=" + Utils::urlEncode(query.query);

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

Result TorrentsCsvProvider::search(const SearchQuery &query, SearchResponse &response)
{
	std::string url = buildSearchUrl(query);
	std::cout << "[" << getName() << "] Searching with URL: " << url << std::endl;
	std::string httpResponse;

	Result httpResult = makeHttpRequest(url, httpResponse, timeoutSeconds, maxRetries);
	if (!httpResult)
	{
		return httpResult;
	}

	return parseSearchResponse(httpResponse, response);
}

Result TorrentsCsvProvider::parseSearchResponse(const std::string &response, SearchResponse &searchResponse)
{
	try
	{
		json j = json::parse(response);

		// Handle different possible response formats
		json torrentsArray;

		if (j.is_array())
		{
			torrentsArray = j;
		}
		else if (j.contains("torrents") && j["torrents"].is_array())
		{
			torrentsArray = j["torrents"];
		}
		else if (j.contains("data") && j["data"].is_array())
		{
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
				continue;
			}

			// Parse infohash field
			auto itInfoHash = item.find("infohash");
			if (itInfoHash != item.end() && itInfoHash->is_string())
			{
				result.infoHash = itInfoHash->get<std::string>();
			}
			else
			{
				continue;
			}

			// Skip duplicates
			if (seenHashes.find(result.infoHash) != seenHashes.end())
			{
				continue;
			}
			seenHashes.insert(result.infoHash);

			// Generate magnet URI
			result.magnetUri = Utils::formatMagnetUri(result.infoHash, result.name);

			// Parse numeric fields
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

			auto itCreatedUnix = item.find("created_unix");
			if (itCreatedUnix != item.end() && itCreatedUnix->is_number())
			{
				result.dateUploaded = std::to_string(itCreatedUnix->get<long long>());
				result.createdUnix = itCreatedUnix->get<long long>();
			}
			else
			{
				result.dateUploaded = "";
				result.createdUnix = 0;
			}

			auto itScrapedDate = item.find("scraped_date");
			if (itScrapedDate != item.end() && itScrapedDate->is_number())
			{
				result.scrapedDate = itScrapedDate->get<long long>();
			}
			else
			{
				result.scrapedDate = 0;
			}

			auto itCompleted = item.find("completed");
			if (itCompleted != item.end() && itCompleted->is_number())
			{
				result.completed = itCompleted->get<int>();
			}
			else
			{
				result.completed = 0;
			}

			result.category = "General";
			searchResponse.torrents.push_back(result);
		}

		std::cout << "[" << getName() << "] Parsed " << searchResponse.torrents.size() << " results" << std::endl;
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

// ============================================================================
// BitsearchProvider implementation
// ============================================================================

BitsearchProvider::BitsearchProvider()
	: apiUrl("https://bitsearch.to/api/search"),
	  timeoutSeconds(30),
	  maxRetries(3)
{
}

std::string BitsearchProvider::buildSearchUrl(const SearchQuery &query) const
{
	std::string url = apiUrl + "?q=" + Utils::urlEncode(query.query);

	// Bitsearch uses 'limit' instead of 'size'
	if (query.maxResults > 0)
	{
		url += "&limit=" + std::to_string(query.maxResults);
	}

	// Pagination using page number
	if (!query.nextToken.empty())
	{
		url += "&page=" + query.nextToken;
	}

	return url;
}

Result BitsearchProvider::search(const SearchQuery &query, SearchResponse &response)
{
	std::string url = buildSearchUrl(query);
	std::cout << "[" << getName() << "] Searching with URL: " << url << std::endl;
	std::string httpResponse;

	Result httpResult = makeHttpRequest(url, httpResponse, timeoutSeconds, maxRetries);
	if (!httpResult)
	{
		return httpResult;
	}

	return parseSearchResponse(httpResponse, response);
}

Result BitsearchProvider::parseSearchResponse(const std::string &response, SearchResponse &searchResponse)
{
	try
	{
		json j = json::parse(response);

		// Bitsearch returns: {data: [{results}], count: N}
		json torrentsArray;

		if (j.contains("data") && j["data"].is_array())
		{
			torrentsArray = j["data"];
		}
		else if (j.is_array())
		{
			torrentsArray = j;
		}
		else
		{
			return Result::Failure("Invalid response format from Bitsearch");
		}

		// Track seen infohashes
		std::set<std::string> seenHashes;
		for (const auto &existing : searchResponse.torrents)
		{
			seenHashes.insert(existing.infoHash);
		}

		for (const auto &item : torrentsArray)
		{
			TorrentSearchResult result;

			// Parse fields (Bitsearch has different field names)
			auto itName = item.find("name");
			if (itName == item.end() || !itName->is_string())
			{
				itName = item.find("title");
			}
			if (itName != item.end() && itName->is_string())
			{
				result.name = itName->get<std::string>();
			}
			else
			{
				continue;
			}

			// Parse infohash
			auto itInfoHash = item.find("infohash");
			if (itInfoHash == item.end() || !itInfoHash->is_string())
			{
				itInfoHash = item.find("hash");
			}
			if (itInfoHash != item.end() && itInfoHash->is_string())
			{
				result.infoHash = itInfoHash->get<std::string>();
			}
			else
			{
				continue;
			}

			// Skip duplicates
			if (seenHashes.find(result.infoHash) != seenHashes.end())
			{
				continue;
			}
			seenHashes.insert(result.infoHash);

			result.magnetUri = Utils::formatMagnetUri(result.infoHash, result.name);

			// Parse size (may be in different field)
			auto itSize = item.find("size");
			if (itSize != item.end() && itSize->is_number())
			{
				result.sizeBytes = itSize->get<size_t>();
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

			auto itCategory = item.find("category");
			if (itCategory != item.end() && itCategory->is_string())
			{
				result.category = itCategory->get<std::string>();
			}
			else
			{
				result.category = "General";
			}

			result.dateUploaded = "";
			result.createdUnix = 0;
			result.scrapedDate = 0;
			result.completed = 0;

			searchResponse.torrents.push_back(result);
		}

		// Check if there are more pages
		if (j.contains("has_more") && j["has_more"].is_boolean())
		{
			searchResponse.hasMore = j["has_more"].get<bool>();
			if (searchResponse.hasMore)
			{
				// Calculate next page number
				int currentPage = 1;
				if (!searchResponse.nextToken.empty())
				{
					try
					{
						currentPage = std::stoi(searchResponse.nextToken);
					}
					catch (...)
					{
					}
				}
				searchResponse.nextToken = std::to_string(currentPage + 1);
			}
		}

		std::cout << "[" << getName() << "] Parsed " << searchResponse.torrents.size() << " results" << std::endl;
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

// ============================================================================
// MultiProvider implementation
// ============================================================================

MultiProvider::MultiProvider()
{
	// Initialize with default providers
	addProvider(std::make_shared<TorrentsCsvProvider>());
	addProvider(std::make_shared<BitsearchProvider>());
}

void MultiProvider::addProvider(std::shared_ptr<SearchProvider> provider)
{
	if (provider && provider->isAvailable())
	{
		providers.push_back(provider);
	}
}

void MultiProvider::setTimeout(int seconds)
{
	for (auto &provider : providers)
	{
		provider->setTimeout(seconds);
	}
}

void MultiProvider::setMaxRetries(int retries)
{
	for (auto &provider : providers)
	{
		provider->setMaxRetries(retries);
	}
}

Result MultiProvider::search(const SearchQuery &query, SearchResponse &response)
{
	std::cout << "[MultiProvider] Searching " << providers.size() << " providers..." << std::endl;

	bool anySuccess = false;
	std::string lastError;
	std::set<std::string> seenHashes;

	for (auto &provider : providers)
	{
		SearchResponse providerResponse;
		Result result = provider->search(query, providerResponse);

		if (result)
		{
			anySuccess = true;
			// Merge results, avoiding duplicates
			for (auto &torrent : providerResponse.torrents)
			{
				if (seenHashes.find(torrent.infoHash) == seenHashes.end())
				{
					seenHashes.insert(torrent.infoHash);
					response.torrents.push_back(torrent);
				}
			}
		}
		else
		{
			lastError = result.getError();
			std::cout << "[MultiProvider] Provider " << provider->getName()
					  << " failed: " << lastError << std::endl;
		}
	}

	if (!anySuccess)
	{
		return Result::Failure("All providers failed. Last error: " + lastError);
	}

	// Sort by seeders (descending)
	std::sort(response.torrents.begin(), response.torrents.end(),
			  [](const TorrentSearchResult &a, const TorrentSearchResult &b)
			  {
				  return a.seeders > b.seeders;
			  });

	std::cout << "[MultiProvider] Total results: " << response.torrents.size() << std::endl;
	return Result::Success();
}

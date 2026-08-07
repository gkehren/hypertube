#include <gtest/gtest.h>
#include "SearchEngine.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <climits>

// Define the test class to be a friend
class SearchEngineTest : public ::testing::Test {
protected:
    SearchEngine engine;

    // Helper to access private method (overload 1)
    Result parseResponse(const std::string& response, std::vector<TorrentSearchResult>& results) {
        return engine.parseSearchResponse(response, results);
    }

    // Helper to access private method (overload 2)
    Result parseResponse(const std::string& response, SearchResponse& searchResponse) {
        return engine.parseSearchResponse(response, searchResponse);
    }

	Result parseTorznab(const std::string& response, SearchResponse& searchResponse) {
		return engine.parseTorznabResponse(response, searchResponse);
	}

    // Helper to access private buildSearchUrl
    std::string buildSearchUrl(const SearchQuery& query) {
        return engine.buildSearchUrl(query);
    }
};

TEST_F(SearchEngineTest, ParseValidArrayResponse) {
    std::string json = R"([
        {
            "name": "Ubuntu 20.04",
            "infohash": "d16a695c02410a0058987b7a5444b059345c2496",
            "size_bytes": 2000000000,
            "seeders": 100,
            "leechers": 10
        }
    ])";

    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Ubuntu 20.04");
    EXPECT_EQ(results[0].infoHash, "d16a695c02410a0058987b7a5444b059345c2496");
    EXPECT_EQ(results[0].seeders, 100);
    EXPECT_EQ(results[0].leechers, 10);
    EXPECT_EQ(results[0].sizeBytes, 2000000000);

    // Verify magnet URI generation
    EXPECT_EQ(results[0].magnetUri.substr(0, 20), "magnet:?xt=urn:btih:");
    EXPECT_NE(results[0].magnetUri.find("d16a695c02410a0058987b7a5444b059345c2496"), std::string::npos);
}

TEST_F(SearchEngineTest, ParseValidObjectWithTorrents) {
    std::string json = R"({
        "torrents": [
            {
                "name": "Arch Linux",
                "infohash": "a1b2c3d4e5f60718293a4b5c6d7e8f9012345678",
                "size_bytes": 800000000,
                "seeders": 50,
                "leechers": 5
            }
        ]
    })";

    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Arch Linux");
    EXPECT_EQ(results[0].infoHash, "a1b2c3d4e5f60718293a4b5c6d7e8f9012345678");
}

TEST_F(SearchEngineTest, ParseValidObjectWithData) {
    std::string json = R"({
        "data": [
            {
                "name": "Debian",
                "infohash": "1234567890abcdef1234567890abcdef12345678",
                "size_bytes": 600000000,
                "seeders": 30,
                "leechers": 3
            }
        ]
    })";

    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Debian");
}

TEST_F(SearchEngineTest, ParseEmptyResponse) {
    std::string json = "";
    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_FALSE(res.success); // Should fail on empty string
}

TEST_F(SearchEngineTest, ParseMalformedJSON) {
    std::string json = "{ invalid json }";
    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_FALSE(res.success);
}

TEST_F(SearchEngineTest, ParseInvalidStructure) {
    std::string json = R"({"foo": "bar"})"; // Valid JSON but no torrents
    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_FALSE(res.success); // Should fail with "Invalid response format"
}

TEST_F(SearchEngineTest, ParseMissingFields) {
    std::string json = R"([
        {
            "name": "Missing InfoHash",
            "size_bytes": 100
        },
        {
            "infohash": "validhashbutnoname",
            "size_bytes": 100
        },
        {
            "name": "Valid Item",
			"infohash": "0123456789abcdef0123456789abcdef01234567",
            "size_bytes": 100
        }
    ])";

    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(results.size(), 1); // Only the valid item should be parsed
    EXPECT_EQ(results[0].name, "Valid Item");
}

TEST_F(SearchEngineTest, ParsePagination) {
    std::string json = R"({
        "torrents": [
            {
                "name": "Item 1",
				"infohash": "1111111111111111111111111111111111111111",
                "size_bytes": 100
            }
        ],
        "next": 12345
    })";

    SearchResponse response;
    Result res = parseResponse(json, response);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(response.torrents.size(), 1);
    ASSERT_TRUE(response.hasMore);
    EXPECT_EQ(response.nextToken, "12345");
}

TEST_F(SearchEngineTest, ParsePaginationStringToken) {
    std::string json = R"({
        "torrents": [],
        "next": "token_string"
    })";

    SearchResponse response;
    Result res = parseResponse(json, response);

    ASSERT_TRUE(res.success);
    ASSERT_TRUE(response.hasMore);
    EXPECT_EQ(response.nextToken, "token_string");
}

TEST_F(SearchEngineTest, ParseDuplicates) {
    std::string json = R"([
        {
            "name": "Item 1",
			"infohash": "1111111111111111111111111111111111111111",
            "size_bytes": 100
        },
        {
            "name": "Item 1 Duplicate",
			"infohash": "1111111111111111111111111111111111111111",
            "size_bytes": 100
        }
    ])";

    SearchResponse response;
    Result res = parseResponse(json, response);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(response.torrents.size(), 1); // Should only have 1 due to deduplication in overload 2
}

TEST_F(SearchEngineTest, ParseNumericFields) {
    std::string json = R"([
        {
            "name": "Item",
			"infohash": "2222222222222222222222222222222222222222",
            "size_bytes": 1024,
            "seeders": 10,
            "leechers": 5,
            "created_unix": 1600000000,
            "scraped_date": 1600000001,
            "completed": 100
        }
    ])";

    std::vector<TorrentSearchResult> results;
    Result res = parseResponse(json, results);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].sizeBytes, 1024);
    EXPECT_EQ(results[0].seeders, 10);
    EXPECT_EQ(results[0].leechers, 5);
    EXPECT_EQ(results[0].createdUnix, 1600000000);
    EXPECT_EQ(results[0].scrapedDate, 1600000001);
    EXPECT_EQ(results[0].completed, 100);
    EXPECT_EQ(results[0].dateUploaded, "1600000000");
}

TEST_F(SearchEngineTest, ClampsOrClearsInvalidNumericFieldsWithoutDroppingResult) {
	std::string json = R"([{
		"name": "Malformed counters",
		"infohash": "3333333333333333333333333333333333333333",
		"size_bytes": -1,
		"seeders": 999999999999,
		"leechers": "unknown",
		"created_unix": -10,
		"completed": -2
	}])";

	std::vector<TorrentSearchResult> results;
	ASSERT_TRUE(parseResponse(json, results));
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().sizeBytes, 0u);
	EXPECT_EQ(results.front().seeders, INT_MAX);
	EXPECT_EQ(results.front().leechers, 0);
	EXPECT_EQ(results.front().createdUnix, 0);
	EXPECT_EQ(results.front().completed, 0);
}

TEST_F(SearchEngineTest, BuildSearchUrlEncodesNextToken) {
    SearchQuery query("ubuntu", 50, "123&attacker=parameter");
    std::string url = buildSearchUrl(query);

    // The nextToken should be URL-encoded, so "&attacker=parameter" becomes "%26attacker%3Dparameter"
    EXPECT_NE(url.find("after=123%26attacker%3Dparameter"), std::string::npos);
    EXPECT_EQ(url.find("&attacker=parameter"), std::string::npos);
}

TEST_F(SearchEngineTest, CustomProviderCanBeSelected) {
    ASSERT_TRUE(engine.registerSearchProvider(
        "local-fixture",
        [](const SearchQuery &query, SearchResponse &response, const std::function<bool()> &cancelled) {
            if (cancelled()) {
                return Result::Failure("cancelled");
            }
            response.torrents.emplace_back(
                query.query, "magnet:?xt=urn:btih:fixture", "fixture", 123, 4, 1, "", "Test");
            return Result::Success();
        }));
    ASSERT_TRUE(engine.setActiveSearchProvider("local-fixture"));

    SearchResponse response;
    ASSERT_TRUE(engine.searchTorrents(SearchQuery("fixture"), response));
    ASSERT_EQ(response.torrents.size(), 1);
    EXPECT_EQ(response.torrents.front().name, "fixture");
    EXPECT_EQ(engine.getActiveSearchProvider(), "local-fixture");
}

TEST_F(SearchEngineTest, ReusesBoundedCacheForIdenticalProviderQuery) {
	int calls = 0;
	ASSERT_TRUE(engine.registerSearchProvider(
		"cached-fixture",
		[&](const SearchQuery &, SearchResponse &response, const std::function<bool()> &) {
			++calls;
			response.torrents.emplace_back("Cached", "magnet:?xt=urn:btih:fixture", "fixture", 1, 1, 0, "", "Test");
			return Result::Success();
		}));
	ASSERT_TRUE(engine.setActiveSearchProvider("cached-fixture"));

	SearchResponse first;
	SearchResponse second;
	ASSERT_TRUE(engine.searchTorrents(SearchQuery("same"), first));
	ASSERT_TRUE(engine.searchTorrents(SearchQuery("same"), second));
	EXPECT_EQ(calls, 1);
	ASSERT_EQ(second.torrents.size(), 1u);
}

TEST_F(SearchEngineTest, AsyncSearchRejectsConcurrentRequestAndPublishesCompletion) {
    std::mutex mutex;
    std::condition_variable enteredCv;
    bool entered = false;
    bool release = false;
    ASSERT_TRUE(engine.registerSearchProvider(
        "blocking-fixture",
        [&](const SearchQuery &query, SearchResponse &response, const std::function<bool()> &cancelled) {
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            enteredCv.notify_one();
            enteredCv.wait(lock, [&] { return release || cancelled(); });
            if (cancelled())
                return Result::Failure("cancelled", ResultCode::Cancelled);
            response.torrents.emplace_back(query.query, "magnet:?xt=urn:btih:fixture", "fixture", 1, 1, 0, "", "Test");
            return Result::Success();
        }));
    ASSERT_TRUE(engine.setActiveSearchProvider("blocking-fixture"));

    uint64_t firstRequest = 0;
    ASSERT_TRUE(engine.startSearch(SearchQuery("first"), firstRequest));
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(enteredCv.wait_for(lock, std::chrono::seconds(2), [&] { return entered; }));
    }

    uint64_t rejectedRequest = 0;
    Result busy = engine.startSearch(SearchQuery("second"), rejectedRequest);
    EXPECT_FALSE(busy);
    EXPECT_EQ(busy.code, ResultCode::Busy);

    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    enteredCv.notify_one();

    std::optional<CompletedSearch> completion;
    for (int i = 0; i < 200 && !completion; ++i) {
        completion = engine.takeCompletedSearch();
        if (!completion)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->requestId, firstRequest);
    EXPECT_TRUE(completion->result);
    ASSERT_EQ(completion->response.torrents.size(), 1);
}

TEST_F(SearchEngineTest, ShutdownCancelsAndJoinsActiveSearch) {
    ASSERT_TRUE(engine.registerSearchProvider(
        "cancel-fixture",
        [](const SearchQuery &, SearchResponse &, const std::function<bool()> &cancelled) {
            while (!cancelled())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return Result::Failure("cancelled", ResultCode::Cancelled);
        }));
    ASSERT_TRUE(engine.setActiveSearchProvider("cancel-fixture"));

    uint64_t requestId = 0;
    ASSERT_TRUE(engine.startSearch(SearchQuery("cancel me"), requestId));
    engine.shutdown();

    EXPECT_FALSE(engine.isSearching());
    auto completion = engine.takeCompletedSearch();
    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->requestId, requestId);
    EXPECT_EQ(completion->result.code, ResultCode::Cancelled);

    uint64_t rejectedRequest = 0;
    Result stopped = engine.startSearch(SearchQuery("too late"), rejectedRequest);
    EXPECT_FALSE(stopped);
    EXPECT_EQ(stopped.code, ResultCode::Unavailable);
}

TEST_F(SearchEngineTest, ParsesTorznabResultsAndPagination) {
	const std::string xml = R"(<?xml version="1.0"?>
	<rss xmlns:newznab="http://www.newznab.com/DTD/2010/feeds/attributes/" xmlns:torznab="http://torznab.com/schemas/2015/feed">
	<channel><newznab:response offset="0" total="2"/>
	<item><title>Linux &amp; Tools</title><guid>0123456789abcdef0123456789abcdef01234567</guid>
	<size>2048</size><category>PC</category>
	<torznab:attr name="seeders" value="12"/><torznab:attr name="peers" value="4"/>
	<torznab:attr name="magneturl" value="magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&amp;dn=Linux"/>
	</item></channel></rss>)";

	SearchResponse response;
	ASSERT_TRUE(parseTorznab(xml, response));
	ASSERT_EQ(response.torrents.size(), 1u);
	EXPECT_EQ(response.torrents.front().name, "Linux & Tools");
	EXPECT_EQ(response.torrents.front().seeders, 12);
	EXPECT_EQ(response.torrents.front().leechers, 4);
	EXPECT_TRUE(response.hasMore);
	EXPECT_EQ(response.nextToken, "1");
}

TEST_F(SearchEngineTest, RejectsInvalidTorznabConfigurationAndProviderErrors) {
	Result invalid = engine.configureTorznabProvider("file:///tmp/feed");
	EXPECT_FALSE(invalid);
	EXPECT_EQ(invalid.code, ResultCode::InvalidInput);

	SearchResponse response;
	Result providerError = parseTorznab("<error code=\"100\" description=\"Bad API key\"/>", response);
	EXPECT_FALSE(providerError);
	EXPECT_EQ(providerError.code, ResultCode::Unavailable);
	EXPECT_EQ(providerError.message, "Bad API key");
}

TEST_F(SearchEngineTest, ValidatesProxyConfiguration) {
	Result missingHost = engine.setProxyConfig(true, "socks5", "", 1080);
	EXPECT_FALSE(missingHost);
	EXPECT_EQ(missingHost.code, ResultCode::InvalidInput);

	Result invalidPort = engine.setProxyConfig(true, "http", "proxy.local", 70000);
	EXPECT_FALSE(invalidPort);
	EXPECT_EQ(invalidPort.code, ResultCode::InvalidInput);

	Result invalidType = engine.setProxyConfig(false, "ftp", "", 1);
	EXPECT_FALSE(invalidType);
	EXPECT_EQ(invalidType.code, ResultCode::InvalidInput);

	EXPECT_TRUE(engine.setProxyConfig(true, "socks5", "127.0.0.1", 1080, "user", "secret"));
	EXPECT_TRUE(engine.setProxyConfig(false, "socks5", "", 1080));
}

TEST_F(SearchEngineTest, ValidatesPreferencesWithoutMutatingRuntimeState) {
	EXPECT_FALSE(SearchEngine::validateTorznabConfig("localhost:9117/api").success);
	EXPECT_FALSE(SearchEngine::validateTorznabConfig("https://").success);
	EXPECT_TRUE(SearchEngine::validateTorznabConfig("https://localhost:9117/api/v2.0/indexers/all/results/torznab").success);

	EXPECT_FALSE(SearchEngine::validateProxyConfig(true, "socks5", "", 1080).success);
	EXPECT_FALSE(SearchEngine::validateProxyConfig(true, "http", "localhost", 70000).success);
	EXPECT_TRUE(SearchEngine::validateProxyConfig(false, "socks5", "", 1080).success);
}

TEST_F(SearchEngineTest, TorznabXmlParsesCdataAndCustomNamespaces) {
	const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
	<rss version="2.0" xmlns:t="http://torznab.com/schemas/2015/feed">
	<channel>
		<item>
			<title><![CDATA[Ubuntu 24.04 LTS Desktop]]></title>
			<guid>1111222233334444555566667777888899990000</guid>
			<size>4294967296</size>
			<category>Linux/OS</category>
			<t:attr name="seeders" value="50"/>
			<t:attr name="peers" value="10"/>
			<t:attr name="infohash" value="1111222233334444555566667777888899990000"/>
		</item>
	</channel>
	</rss>)";

	SearchResponse response;
	ASSERT_TRUE(parseTorznab(xml, response));
	ASSERT_EQ(response.torrents.size(), 1u);
	EXPECT_EQ(response.torrents[0].name, "Ubuntu 24.04 LTS Desktop");
	EXPECT_EQ(response.torrents[0].infoHash, "1111222233334444555566667777888899990000");
	EXPECT_EQ(response.torrents[0].seeders, 50);
	EXPECT_EQ(response.torrents[0].leechers, 10);
}

TEST_F(SearchEngineTest, GetFavoriteHashesSetReturnsSnapshot) {
	TorrentSearchResult t1;
	t1.name = "Test Torrent 1";
	t1.infoHash = "0000000000000000000000000000000000000001";
	engine.addToFavorites(t1);

	const auto favSet = engine.getFavoriteHashesSet();
	EXPECT_EQ(favSet.size(), 1u);
	EXPECT_EQ(favSet.count("0000000000000000000000000000000000000001"), 1u);
}

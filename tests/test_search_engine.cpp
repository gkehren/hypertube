#include <gtest/gtest.h>
#include "SearchEngine.hpp"
#include <vector>
#include <string>

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
            "infohash": "validhashvaliditem",
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
                "infohash": "hash1",
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
            "infohash": "hash1",
            "size_bytes": 100
        },
        {
            "name": "Item 1 Duplicate",
            "infohash": "hash1",
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
            "infohash": "hash",
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

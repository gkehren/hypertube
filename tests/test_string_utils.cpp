#include <gtest/gtest.h>
#include "StringUtils.hpp"
#include <cstring>
#include <string>

namespace {

TEST(StringUtilsTest, FormatBytesBytes) {
    char buf[64];
    Utils::formatBytes(500, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "500 B");
}

TEST(StringUtilsTest, FormatBytesKB) {
    char buf[64];
    Utils::formatBytes(1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 KB");

    Utils::formatBytes(1536, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1.5 KB"); // 1536/1024 = 1.5

    // New precision test
    Utils::formatBytes(1556, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1.52 KB"); // 1556/1024 = 1.5195... -> 1.52
}

TEST(StringUtilsTest, FormatBytesMB) {
    char buf[64];
    Utils::formatBytes(1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 MB");

    Utils::formatBytes(1572864, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1.5 MB");
}

TEST(StringUtilsTest, FormatBytesGB) {
    char buf[64];
    Utils::formatBytes(1024ULL * 1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 GB");

    Utils::formatBytes(1536ULL * 1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1.5 GB");
}

TEST(StringUtilsTest, FormatBytesSpeed) {
    char buf[64];
    Utils::formatBytes(1024, true, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 KB/s");

    Utils::formatBytes(1536, true, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1.5 KB/s");
}

TEST(StringUtilsTest, UrlEncodeAlphanumeric) {
    std::string input = "HelloWorld123";
    std::string expected = "HelloWorld123";
    EXPECT_EQ(Utils::urlEncode(input), expected);
}

TEST(StringUtilsTest, UrlEncodeSpecialChars) {
    std::string input = "Hello World!";
    std::string expected = "Hello%20World%21";
    EXPECT_EQ(Utils::urlEncode(input), expected);
}

TEST(StringUtilsTest, UrlEncodeReservedChars) {
    std::string input = "-_.~";
    std::string expected = "-_.~";
    EXPECT_EQ(Utils::urlEncode(input), expected);
}

TEST(StringUtilsTest, UrlEncodeAllReserved) {
    std::string input = ":/?#[]@!$&'()*+,;=";
    // Expected encoding for each char
    std::string expected = "%3A%2F%3F%23%5B%5D%40%21%24%26%27%28%29%2A%2B%2C%3B%3D";
    EXPECT_EQ(Utils::urlEncode(input), expected);
}

TEST(StringUtilsTest, FormatMagnetUriBasic) {
    std::string infoHash = "1234567890abcdef1234567890abcdef12345678";
    std::string name = "My Torrent";
    std::string magnet = Utils::formatMagnetUri(infoHash, name);

    EXPECT_TRUE(magnet.find("magnet:?xt=urn:btih:" + infoHash) == 0);
    EXPECT_TRUE(magnet.find("&dn=My%20Torrent") != std::string::npos);
    // Check for trackers
    EXPECT_TRUE(magnet.find("&tr=udp://tracker.openbittorrent.com:80") != std::string::npos);
}

TEST(StringUtilsTest, FormatMagnetUriEmptyName) {
    std::string infoHash = "1234567890abcdef1234567890abcdef12345678";
    std::string name = "";
    std::string magnet = Utils::formatMagnetUri(infoHash, name);

    EXPECT_TRUE(magnet.find("magnet:?xt=urn:btih:" + infoHash) == 0);
    EXPECT_TRUE(magnet.find("&dn=") == std::string::npos);
    EXPECT_TRUE(magnet.find("&tr=") != std::string::npos);
}

TEST(StringUtilsTest, FormatBytesZero) {
    char buf[64];
    Utils::formatBytes(0, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "0 B");

    Utils::formatBytes(0, true, buf, sizeof(buf));
    EXPECT_STREQ(buf, "0 B/s");
}

TEST(StringUtilsTest, FormatBytesLarge) {
    char buf[64];
    // 1 TB
    Utils::formatBytes((size_t)1024 * 1024 * 1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 TB");
}

TEST(StringUtilsTest, ComputeETA) {
    char buf[64];
    std::memset(buf, 0, sizeof(buf));
    lt::torrent_status status;

    // Not downloading
    status.state = lt::torrent_status::seeding;
    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "N/A");

    // Downloading but zero rate
    status.state = lt::torrent_status::downloading;
    status.download_payload_rate = 0;
    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "N/A");

    // Seconds
    status.download_payload_rate = 100;
    status.total_wanted = 1000;
    status.total_wanted_done = 500; // 500 / 100 = 5s
    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "5 seconds");

    // Minutes
    status.download_payload_rate = 10;
    status.total_wanted = 2000;
    status.total_wanted_done = 1000; // 1000 / 10 = 100s = 1m 40s -> 1m
    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 minutes");

    // Hours
    status.download_payload_rate = 10;
    status.total_wanted = 73000;
    status.total_wanted_done = 1000; // 72000 / 10 = 7200s = 120m = 2h
    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "2 hours");

    // Days
    status.download_payload_rate = 1;
    status.total_wanted = 172800;
    status.total_wanted_done = 0; // 172800s = 2880m = 48h = 2d
    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "2 days");
}

TEST(StringUtilsTest, ComputeETABufferTruncation) {
    char buf[4];
    std::memset(buf, 0, sizeof(buf));
    lt::torrent_status status;
    status.state = lt::torrent_status::downloading;
    status.download_payload_rate = 100;
    status.total_wanted = 1000;
    status.total_wanted_done = 500; // 5 seconds

    Utils::computeETA(status, buf, sizeof(buf));
    EXPECT_STREQ(buf, "5 s"); // "5 s" + null
}

TEST(StringUtilsTest, FormatBytesBufferTruncation) {
    char buf[4]; // Size 4: can hold 3 chars + null
    // "1 KB" needs 5 chars (4 + null) to fit completely.
    // snprintf will write "1 K" (3 chars) + null terminator.
    Utils::formatBytes(1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 K");
}

} // namespace

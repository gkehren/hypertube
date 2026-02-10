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
    EXPECT_STREQ(buf, "1 KB"); // Integer division 1536/1024 = 1
}

TEST(StringUtilsTest, FormatBytesMB) {
    char buf[64];
    Utils::formatBytes(1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 MB");
}

TEST(StringUtilsTest, FormatBytesGB) {
    char buf[64];
    Utils::formatBytes(1024ULL * 1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 GB");
}

TEST(StringUtilsTest, FormatBytesSpeed) {
    char buf[64];
    Utils::formatBytes(1024, true, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 KB/s");
}

TEST(StringUtilsTest, FormatBytesZero) {
    char buf[64];
    Utils::formatBytes(0, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "0 B");

    Utils::formatBytes(0, true, buf, sizeof(buf));
    EXPECT_STREQ(buf, "0 B/s");
}

TEST(StringUtilsTest, FormatBytesBoundaries) {
    char buf[64];
    // 1023 B
    Utils::formatBytes(1023, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1023 B");

    // Just below 1 MB
    Utils::formatBytes(1024 * 1024 - 1, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1023 KB");

    // Just below 1 GB
    Utils::formatBytes((size_t)1024 * 1024 * 1024 - 1, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1023 MB");
}

TEST(StringUtilsTest, FormatBytesLarge) {
    char buf[64];
    // 1 TB
    Utils::formatBytes((size_t)1024 * 1024 * 1024 * 1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 TB");
}

TEST(StringUtilsTest, FormatBytesBufferTruncation) {
    char buf[4]; // Size 4: can hold 3 chars + null
    // "1 KB" needs 5 chars (4 + null) to fit completely.
    // snprintf will write "1 K" (3 chars) + null terminator.
    Utils::formatBytes(1024, false, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1 K");
}

} // namespace

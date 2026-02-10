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

} // namespace

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

} // namespace

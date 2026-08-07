#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace Presentation::UiFormatters
{
std::string formatBytes(std::int64_t bytes, bool speed = false);
std::string formatRate(std::int64_t bytesPerSecond);
std::string formatProgress(float progress);
std::string formatEta(std::int64_t seconds);
std::string formatUnixDate(std::int64_t unixTime);
std::string formatRatio(int numerator, int denominator);
std::string formatCount(std::int64_t count);
std::string formatTimestamp(const std::chrono::system_clock::time_point &time);
std::string torrentStateToString(int state, bool paused, bool finished);
} // namespace Presentation::UiFormatters

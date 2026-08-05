#include "presentation/UiFormatters.hpp"

#include "StringUtils.hpp"
#include "SystemUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Presentation::UiFormatters
{
std::string formatBytes(std::int64_t bytes, bool speed)
{
	char buffer[64]{};
	Utils::formatBytes(bytes < 0 ? 0 : static_cast<std::size_t>(bytes), speed, buffer, sizeof(buffer));
	return buffer;
}

std::string formatRate(std::int64_t bytesPerSecond)
{
	return formatBytes(bytesPerSecond, true);
}

std::string formatProgress(float progress)
{
	const float clamped = std::clamp(progress, 0.0f, 1.0f);
	char buffer[32]{};
	std::snprintf(buffer, sizeof(buffer), "%.1f%%", clamped * 100.0f);
	return buffer;
}

std::string formatEta(std::int64_t seconds)
{
	if (seconds < 0)
		return "N/A";
	if (seconds >= 24 * 60 * 60)
		return std::to_string(seconds / (24 * 60 * 60)) + " days";
	if (seconds >= 60 * 60)
		return std::to_string(seconds / (60 * 60)) + " hours";
	if (seconds >= 60)
		return std::to_string(seconds / 60) + " minutes";
	return std::to_string(seconds) + " seconds";
}

std::string formatUnixDate(std::int64_t unixTime)
{
	if (unixTime == 0)
		return "N/A";

	const std::int64_t seconds = unixTime > 1000000000000LL ? unixTime / 1000 : unixTime;
	if (seconds < 0 || seconds > 4102444800LL)
		return "Invalid TS";

	std::tm local{};
	const std::time_t time = static_cast<std::time_t>(seconds);
	if (!Utils::SystemUtils::getLocalTime(time, local))
		return "TM Error";

	char buffer[32]{};
	if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local) == 0)
		return "Format Error";
	return buffer;
}

std::string formatRatio(int numerator, int denominator)
{
	if (denominator > 0)
	{
		std::ostringstream output;
		output << std::fixed << std::setprecision(1)
		       << static_cast<double>(numerator) / static_cast<double>(denominator);
		return output.str();
	}
	return numerator > 0 ? "∞" : "-";
}

std::string formatCount(std::int64_t count)
{
	return std::to_string(std::max<std::int64_t>(0, count));
}

std::string formatTimestamp(const std::chrono::system_clock::time_point &time)
{
	const auto timeValue = std::chrono::system_clock::to_time_t(time);
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
	std::tm local{};
	if (!Utils::SystemUtils::getLocalTime(timeValue, local))
		return "00:00:00.000";

	std::ostringstream output;
	output << std::put_time(&local, "%H:%M:%S")
	       << '.' << std::setfill('0') << std::setw(3) << milliseconds.count();
	return output.str();
}

std::string torrentStateToString(int state, bool paused, bool finished)
{
	if (paused)
		return "Paused";
	if (finished)
		return "Finished";

	// libtorrent's state enum is intentionally mapped at the presenter boundary.
	// Keeping this function primitive-only makes it usable by every frontend.
	switch (state)
	{
	case 1:
		return "Checking files";
	case 2:
		return "Downloading metadata";
	case 3:
		return "Downloading";
	case 4:
		return "Finished";
	case 5:
		return "Seeding";
	case 7:
		return "Checking resume data";
	default:
		return "Unknown";
	}
}
} // namespace Presentation::UiFormatters

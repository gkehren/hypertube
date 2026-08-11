#include "presentation/UiFormatters.hpp"

#include "StringUtils.hpp"
#include "SystemUtils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <limits>
#include <cctype>

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

Result parseSpeedLimit(const std::string &value, int &bytesPerSecond)
{
	const auto first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return Result::Failure("A speed limit is required; use 0 for unlimited", ResultCode::InvalidInput);
	const auto last = value.find_last_not_of(" \t\r\n");
	const std::string trimmed = value.substr(first, last - first + 1);

	std::size_t numberLength = 0;
	double number = 0.0;
	try
	{
		number = std::stod(trimmed, &numberLength);
	}
	catch (...)
	{
		return Result::Failure("Speed limits must be a non-negative number followed by B/s, KiB/s, MiB/s, or GiB/s",
			ResultCode::InvalidInput);
	}
	if (!std::isfinite(number) || number < 0.0)
		return Result::Failure("Speed limits must be non-negative; use 0 for unlimited", ResultCode::InvalidInput);

	std::string unit = trimmed.substr(numberLength);
	unit.erase(std::remove_if(unit.begin(), unit.end(), [](unsigned char character) {
		return std::isspace(character) != 0;
	}), unit.end());
	std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});

	double multiplier = 1.0;
	if (unit.empty() || unit == "b" || unit == "b/s")
		multiplier = 1.0;
	else if (unit == "kib" || unit == "kib/s")
		multiplier = 1024.0;
	else if (unit == "mib" || unit == "mib/s")
		multiplier = 1024.0 * 1024.0;
	else if (unit == "gib" || unit == "gib/s")
		multiplier = 1024.0 * 1024.0 * 1024.0;
	else
		return Result::Failure("Speed limits must use B/s, KiB/s, MiB/s, or GiB/s", ResultCode::InvalidInput);

	const double scaled = number * multiplier;
	if (!std::isfinite(scaled) || scaled > static_cast<double>(std::numeric_limits<int>::max()))
		return Result::Failure("Speed limit is larger than the supported maximum of 2147483647 B/s",
			ResultCode::InvalidInput);
	const auto rounded = std::llround(scaled);
	if (rounded > std::numeric_limits<int>::max())
		return Result::Failure("Speed limit is larger than the supported maximum of 2147483647 B/s",
			ResultCode::InvalidInput);
	bytesPerSecond = static_cast<int>(rounded);
	return Result::Success();
}

std::string formatSpeedLimit(std::int64_t bytesPerSecond)
{
	const std::int64_t value = std::max<std::int64_t>(0, bytesPerSecond);
	if (value == 0)
		return "0";

	double amount = static_cast<double>(value);
	const char *unit = "B/s";
	std::int64_t unitMultiplier = 1;
	if (value >= 1024LL * 1024LL * 1024LL)
	{
		amount /= 1024.0 * 1024.0 * 1024.0;
		unit = "GiB/s";
		unitMultiplier = 1024LL * 1024LL * 1024LL;
	}
	else if (value >= 1024LL * 1024LL)
	{
		amount /= 1024.0 * 1024.0;
		unit = "MiB/s";
		unitMultiplier = 1024LL * 1024LL;
	}
	else if (value >= 1024)
	{
		amount /= 1024.0;
		unit = "KiB/s";
		unitMultiplier = 1024;
	}

	const int precision = amount >= 10.0 ? 0 : 1;
	const auto displayedAmount = std::llround(amount * std::pow(10.0, precision))
		/ std::pow(10.0, precision);
	if (unitMultiplier > 1 && std::llround(displayedAmount * static_cast<double>(unitMultiplier)) > std::numeric_limits<int>::max())
		return std::to_string(value) + " B/s";

	std::ostringstream output;
	output << std::fixed << std::setprecision(precision) << amount;
	std::string formatted = output.str();
	while (formatted.size() > 1 && formatted.ends_with('0') && formatted.find('.') != std::string::npos)
		formatted.pop_back();
	if (!formatted.empty() && formatted.back() == '.')
		formatted.pop_back();
	return formatted + " " + unit;
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

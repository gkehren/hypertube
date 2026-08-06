#pragma once

#include "Result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Presentation
{
enum class TorrentAvailability
{
	LoadingStatus,
	Available,
	MetadataPending,
	Removed,
	InvalidId,
	Error
};

struct TorrentAvailabilityInfo
{
	TorrentAvailability state = TorrentAvailability::LoadingStatus;
	std::string details;
};

std::string availabilityMessage(const TorrentAvailabilityInfo &availability);
Result availabilityFailure(const TorrentAvailabilityInfo &availability);
void logInvalidTorrentId(std::string_view id, std::uint64_t collectionRevision,
	std::size_t registrySize);
} // namespace Presentation

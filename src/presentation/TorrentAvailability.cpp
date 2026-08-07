#include "presentation/TorrentAvailability.hpp"

#include "Logger.hpp"

#include <algorithm>

namespace Presentation
{
std::string availabilityMessage(const TorrentAvailabilityInfo &availability)
{
	switch (availability.state)
	{
	case TorrentAvailability::LoadingStatus:
		return "Loading torrent status...";
	case TorrentAvailability::Available:
		return {};
	case TorrentAvailability::MetadataPending:
		return "Waiting for torrent metadata...";
	case TorrentAvailability::Removed:
		return "The selected torrent was removed.";
	case TorrentAvailability::InvalidId:
		return "Internal torrent identifier error.";
	case TorrentAvailability::Error:
		return availability.details.empty() ? "Torrent operation failed." : availability.details;
	}
	return "Torrent operation failed.";
}

Result availabilityFailure(const TorrentAvailabilityInfo &availability)
{
	const ResultCode code = availability.state == TorrentAvailability::Removed
		? ResultCode::NotFound : ResultCode::InvalidInput;
	return Result::Failure(availabilityMessage(availability), code);
}

void logInvalidTorrentId(std::string_view id, std::uint64_t collectionRevision,
	std::size_t registrySize)
{
	const std::size_t prefixLength = std::min<std::size_t>(id.size(), 8);
	Utils::Logger::error("torrent", "Invalid UI torrent identifier: length="
		+ std::to_string(id.size()) + ", prefix=" + std::string(id.substr(0, prefixLength))
		+ ", collection_revision=" + std::to_string(collectionRevision)
		+ ", registry_entries=" + std::to_string(registrySize));
}
} // namespace Presentation

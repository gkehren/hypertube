#include "presentation/TorrentListPresenter.hpp"

#include "utils/TorrentIdentity.hpp"
#include "presentation/UiFormatters.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace
{
std::string torrentId(const lt::info_hash_t &hash)
{
	return Utils::TorrentIdentity::id(hash);
}

bool lessOrEqual(const Presentation::TorrentRowDto &left, const Presentation::TorrentRowDto &right,
	Presentation::TorrentSortField field)
{
	switch (field)
	{
	case Presentation::TorrentSortField::Queue:
		return left.queuePosition <= right.queuePosition;
	case Presentation::TorrentSortField::Name:
		return left.name <= right.name;
	case Presentation::TorrentSortField::Size:
		return left.sizeBytes <= right.sizeBytes;
	case Presentation::TorrentSortField::Progress:
		return left.progress <= right.progress;
	case Presentation::TorrentSortField::Status:
		return left.stateLabel <= right.stateLabel;
	case Presentation::TorrentSortField::DownloadRate:
		return left.downloadRateBytes <= right.downloadRateBytes;
	case Presentation::TorrentSortField::UploadRate:
		return left.uploadRateBytes <= right.uploadRateBytes;
	case Presentation::TorrentSortField::Eta:
		return left.etaSeconds <= right.etaSeconds;
	case Presentation::TorrentSortField::Seeds:
		return left.seeds <= right.seeds;
	case Presentation::TorrentSortField::Peers:
		return left.peers <= right.peers;
	}
	return true;
}
} // namespace

namespace Presentation
{
TorrentListPresenter::TorrentListPresenter(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

std::string TorrentListPresenter::idForHash(const lt::info_hash_t &hash)
{
	return torrentId(hash);
}

void TorrentListPresenter::setCategoryFilter(int filter)
{
	categoryFilter_ = std::clamp(filter, 0, 6);
}

void TorrentListPresenter::setTextFilter(std::string filter)
{
	textFilter_ = std::move(filter);
}

void TorrentListPresenter::setSort(TorrentSortField field, bool ascending)
{
	sortField_ = field;
	sortAscending_ = ascending;
}

void TorrentListPresenter::setSelectedId(std::string id)
{
	selectedId_ = std::move(id);
}

const std::vector<TorrentRowDto> &TorrentListPresenter::buildUnfilteredRows()
{
	ensurePresentationCurrent();
	if (!selectedId_.empty() && hashesById_.find(selectedId_) == hashesById_.end())
		selectedId_.clear();
	return presentation_.allRows;
}

void TorrentListPresenter::ensurePresentationCurrent() const
{
	const auto collectionRevision = torrentManager.getTorrentCollectionRevision();
	const auto statusRevision = torrentManager.getStatusRevision();
	if (presentationValid_ && presentation_.collectionRevision == collectionRevision
		&& presentation_.statusRevision == statusRevision)
		return;

	const auto torrents = torrentManager.getTorrentSnapshot();
	const auto statusCache = torrentManager.getStatusCache();
	PresentationSnapshot next;
	next.collectionRevision = collectionRevision;
	next.statusRevision = statusRevision;
	next.allRows.reserve(torrents.size());
	next.hashesById.reserve(torrents.size());

	for (const auto &torrent : torrents)
	{
		if (!torrent.handle.is_valid())
			continue;

		const auto id = torrentId(torrent.hash);
		if (id.empty())
			continue;
		next.hashesById.emplace(id, torrent.hash);
		if (!statusCache || statusCache->find(torrent.hash) == statusCache->end())
		{
			TorrentRowDto row;
			row.id = id;
			row.name = !torrent.displayName.empty() ? torrent.displayName : "Loading torrent...";
			row.progress = 0.0f;
			row.progressLabel = UiFormatters::formatProgress(0.0f);
			row.sizeBytes = 0;
			row.sizeLabel = UiFormatters::formatBytes(0);
			row.downloadRateBytes = 0;
			row.uploadRateBytes = 0;
			row.downloadRateLabel = UiFormatters::formatRate(0);
			row.uploadRateLabel = UiFormatters::formatRate(0);
			row.peers = 0;
			row.seeds = 0;
			row.peersLabel = UiFormatters::formatCount(0);
			row.seedsLabel = UiFormatters::formatCount(0);
			row.queuePosition = -1;
			row.paused = false;
			row.active = false;
			row.finished = false;
			row.error = false;
			row.state = TorrentUiState::Other;
			row.stateLabel = "Loading";
			row.etaSeconds = -1;
			row.etaLabel = UiFormatters::formatEta(-1);
			row.metadataPending = true;
			row.commandsAvailable = true;
			next.allRows.push_back(std::move(row));
			continue;
		}

		const auto &value = statusCache->find(torrent.hash)->second;
		TorrentRowDto row;
		row.id = id;
		row.name = !value.name.empty() ? value.name : (!torrent.displayName.empty() ? torrent.displayName : "Loading torrent...");
		row.progress = std::clamp(value.progress, 0.0f, 1.0f);
		row.progressLabel = UiFormatters::formatProgress(row.progress);
		row.sizeBytes = value.total_wanted;
		row.sizeLabel = UiFormatters::formatBytes(row.sizeBytes);
		row.downloadRateBytes = value.download_payload_rate;
		row.uploadRateBytes = value.upload_payload_rate;
		row.downloadRateLabel = UiFormatters::formatRate(row.downloadRateBytes);
		row.uploadRateLabel = UiFormatters::formatRate(row.uploadRateBytes);
		row.peers = value.num_peers;
		row.seeds = value.num_seeds;
		row.peersLabel = UiFormatters::formatCount(row.peers);
		row.seedsLabel = UiFormatters::formatCount(row.seeds);
		using QueuePosition = std::remove_cv_t<decltype(value.queue_position)>;
		const int queuePosition = static_cast<int>(static_cast<typename QueuePosition::underlying_type>(value.queue_position));
		row.queuePosition = queuePosition < 0 ? -1 : queuePosition + 1;
		row.paused = (value.flags & lt::torrent_flags::paused) != lt::torrent_flags_t{};
		row.active = row.downloadRateBytes > 0 || row.uploadRateBytes > 0;
		row.finished = value.is_finished;
		row.error = static_cast<bool>(value.errc);
		row.metadataPending = value.state == lt::torrent_status::downloading_metadata || value.has_metadata == false;
		row.commandsAvailable = true;
		if (row.paused)
			row.state = TorrentUiState::Paused;
		else if (row.finished)
			row.state = TorrentUiState::Completed;
		else if (value.state == lt::torrent_status::seeding)
			row.state = TorrentUiState::Seeding;
		else if (value.state == lt::torrent_status::downloading || value.state == lt::torrent_status::downloading_metadata)
			row.state = TorrentUiState::Downloading;
		else
			row.state = TorrentUiState::Other;
		row.stateLabel = row.error ? value.errc.message()
			: UiFormatters::torrentStateToString(static_cast<int>(value.state), row.paused, row.finished);
		if (value.state == lt::torrent_status::downloading && row.downloadRateBytes > 0)
		{
			const auto remaining = std::max<std::int64_t>(0, value.total_wanted - value.total_wanted_done);
			row.etaSeconds = remaining / row.downloadRateBytes;
		}
		row.etaLabel = UiFormatters::formatEta(row.etaSeconds);

		next.allRows.push_back(std::move(row));
	}

	for (const auto &row : next.allRows)
	{
		if (row.state == TorrentUiState::Downloading)
			++next.categoryCounts[1];
		if (row.state == TorrentUiState::Seeding)
			++next.categoryCounts[2];
		if (row.state == TorrentUiState::Completed)
			++next.categoryCounts[3];
		if (row.state == TorrentUiState::Paused)
			++next.categoryCounts[4];
		if (row.active)
			++next.categoryCounts[5];
		else
			++next.categoryCounts[6];
	}
	next.categoryCounts[0] = static_cast<int>(next.allRows.size());

	presentation_ = std::move(next);
	hashesById_ = presentation_.hashesById;
	presentationValid_ = true;
}

bool TorrentListPresenter::matchesCategory(const TorrentRowDto &row, int filter)
{
	switch (filter)
	{
	case 0:
		return true;
	case 1:
		return row.state == TorrentUiState::Downloading;
	case 2:
		return row.state == TorrentUiState::Seeding;
	case 3:
		return row.state == TorrentUiState::Completed;
	case 4:
		return row.state == TorrentUiState::Paused;
	case 5:
		return row.active;
	case 6:
		return !row.active;
	default:
		return true;
	}
}

bool TorrentListPresenter::matchesCategory(const TorrentRowDto &row) const
{
	return matchesCategory(row, categoryFilter_);
}

bool TorrentListPresenter::matchesTextFilter(const TorrentRowDto &row) const
{
	if (textFilter_.empty())
		return true;

	std::string name = row.name;
	std::string state = row.stateLabel;
	std::string filter = textFilter_;
	auto lowercase = [](std::string &value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
	};
	lowercase(name);
	lowercase(state);
	lowercase(filter);
	return name.find(filter) != std::string::npos || state.find(filter) != std::string::npos;
}

std::vector<TorrentRowDto> TorrentListPresenter::buildRows()
{
	auto rows = buildUnfilteredRows();
	rows.erase(std::remove_if(rows.begin(), rows.end(), [this](const TorrentRowDto &row)
	{
		return !matchesCategory(row) || !matchesTextFilter(row);
	}), rows.end());

	std::stable_sort(rows.begin(), rows.end(), [this](const TorrentRowDto &left, const TorrentRowDto &right)
	{
		const bool less = lessOrEqual(left, right, sortField_);
		const bool greater = lessOrEqual(right, left, sortField_);
		if (less == greater)
			return left.id < right.id;
		return sortAscending_ ? less : greater;
	});
	return rows;
}

std::optional<TorrentRowDto> TorrentListPresenter::findRowById(const std::string &id)
{
	if (id.empty())
		return std::nullopt;

	// Resolve selections against the unfiltered source of truth. The visible
	// model can be filtered, reordered, or intentionally left untouched until
	// its next status revision without making an existing torrent unavailable.
	for (const auto &row : buildUnfilteredRows())
	{
		if (row.id == id)
			return row;
	}

	// Status refreshes are asynchronous. A live magnet may briefly be absent
	// from the latest status snapshot, but it must remain selectable.
	return std::nullopt;
}

std::vector<CategoryDto> TorrentListPresenter::buildCategories()
{
	ensurePresentationCurrent();

	const std::array<const char *, 7> labels = {
		"All Torrents", "Downloading", "Seeding", "Completed", "Paused", "Active", "Inactive"};
	std::vector<CategoryDto> categories;
	categories.reserve(labels.size());
	for (int id = 0; id < static_cast<int>(labels.size()); ++id)
	{
		CategoryDto category{id, labels[static_cast<std::size_t>(id)], presentation_.categoryCounts[static_cast<std::size_t>(id)]};
		categories.push_back(std::move(category));
	}
	return categories;
}

std::optional<lt::info_hash_t> TorrentListPresenter::hashForId(const std::string &id) const
{
	ensurePresentationCurrent();
	const auto found = hashesById_.find(id);
	if (found != hashesById_.end())
		return found->second;
	return std::nullopt;
}

TorrentAvailabilityInfo TorrentListPresenter::availabilityForId(const std::string &id)
{
	if (!Utils::TorrentIdentity::isValid(id))
		return {TorrentAvailability::InvalidId, {}};
	if (!hashForId(id))
		return {TorrentAvailability::Removed, {}};
	const auto row = findRowById(id);
	if (!row)
		return {TorrentAvailability::LoadingStatus, {}};
	if (row->error)
		return {TorrentAvailability::Error, row->stateLabel};
	if (row->stateLabel.find("metadata") != std::string::npos
		|| row->stateLabel.find("Metadata") != std::string::npos)
		return {TorrentAvailability::MetadataPending, {}};
	if (row->stateLabel == "Loading")
		return {TorrentAvailability::LoadingStatus, {}};
	return {TorrentAvailability::Available, {}};
}

Result TorrentListPresenter::executeCommand(const std::string &id, TorrentCommand command)
{
	const auto hash = hashForId(id);
	if (!hash)
		return availabilityFailure(availabilityForId(id));
	return torrentManager.executeCommand(*hash, command);
}

Result TorrentListPresenter::removeTorrent(const std::string &id, TorrentRemovalMode mode)
{
	const auto hash = hashForId(id);
	if (!hash)
		return availabilityFailure(availabilityForId(id));
	return torrentManager.removeTorrent(*hash, mode);
}
} // namespace Presentation

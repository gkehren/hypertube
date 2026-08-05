#include "presentation/TorrentListPresenter.hpp"

#include "presentation/UiFormatters.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <utility>

namespace
{
std::string torrentId(const lt::info_hash_t &hash)
{
	if (hash.has_v1())
		return hash.v1.to_string();
	if (hash.has_v2())
		return hash.v2.to_string();
	return {};
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

std::vector<TorrentRowDto> TorrentListPresenter::buildUnfilteredRows()
{
	const auto torrents = torrentManager.getTorrentSnapshot();
	const auto statusCache = torrentManager.getStatusCache();
	std::vector<TorrentRowDto> rows;
	rows.reserve(torrents.size());
	hashesById_.clear();

	for (const auto &torrent : torrents)
	{
		if (!torrent.handle.is_valid())
			continue;

		const auto id = torrentId(torrent.hash);
		if (id.empty())
			continue;
		auto status = statusCache ? statusCache->find(torrent.hash) : statusCache->end();
		if (!statusCache || status == statusCache->end())
			continue;

		const auto &value = status->second;
		TorrentRowDto row;
		row.id = id;
		row.name = value.name;
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
		row.queuePosition = value.queue_position < 0 ? -1 : static_cast<int>(value.queue_position) + 1;
		row.paused = (value.flags & lt::torrent_flags::paused) != lt::torrent_flags_t{};
		row.active = row.downloadRateBytes > 0 || row.uploadRateBytes > 0;
		row.finished = value.is_finished;
		row.error = static_cast<bool>(value.errc);
		row.stateLabel = UiFormatters::torrentStateToString(static_cast<int>(value.state), row.paused, row.finished);
		if (value.state == lt::torrent_status::downloading && row.downloadRateBytes > 0)
		{
			const auto remaining = std::max<std::int64_t>(0, value.total_wanted - value.total_wanted_done);
			row.etaSeconds = remaining / row.downloadRateBytes;
		}
		row.etaLabel = UiFormatters::formatEta(row.etaSeconds);

		hashesById_.emplace(row.id, torrent.hash);
		rows.push_back(std::move(row));
	}

	if (!selectedId_.empty() && hashesById_.find(selectedId_) == hashesById_.end())
		selectedId_.clear();
	return rows;
}

bool TorrentListPresenter::matchesCategory(const TorrentRowDto &row) const
{
	switch (categoryFilter_)
	{
	case 0:
		return true;
	case 1:
		return row.stateLabel == "Downloading" || row.stateLabel == "Downloading metadata";
	case 2:
		return row.stateLabel == "Seeding";
	case 3:
		return row.finished;
	case 4:
		return row.paused;
	case 5:
		return row.active;
	case 6:
		return !row.active;
	default:
		return true;
	}
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

std::vector<CategoryDto> TorrentListPresenter::buildCategories()
{
	const int previousFilter = categoryFilter_;
	categoryFilter_ = 0;
	auto rows = buildUnfilteredRows();
	categoryFilter_ = previousFilter;

	const std::array<const char *, 7> labels = {
		"All Torrents", "Downloading", "Seeding", "Completed", "Paused", "Active", "Inactive"};
	std::vector<CategoryDto> categories;
	categories.reserve(labels.size());
	for (int id = 0; id < static_cast<int>(labels.size()); ++id)
	{
		CategoryDto category{id, labels[static_cast<std::size_t>(id)], 0};
		for (const auto &row : rows)
		{
			const int savedFilter = categoryFilter_;
			categoryFilter_ = id;
			if (matchesCategory(row))
				++category.count;
			categoryFilter_ = savedFilter;
		}
		categories.push_back(std::move(category));
	}
	return categories;
}

std::optional<lt::info_hash_t> TorrentListPresenter::hashForId(const std::string &id) const
{
	const auto found = hashesById_.find(id);
	if (found == hashesById_.end())
		return std::nullopt;
	return found->second;
}

Result TorrentListPresenter::executeCommand(const std::string &id, TorrentCommand command)
{
	const auto hash = hashForId(id);
	if (!hash)
		return Result::Failure("Torrent is no longer available", ResultCode::NotFound);
	return torrentManager.executeCommand(*hash, command);
}

Result TorrentListPresenter::removeTorrent(const std::string &id, TorrentRemovalMode mode)
{
	const auto hash = hashForId(id);
	if (!hash)
		return Result::Failure("Torrent is no longer available", ResultCode::NotFound);
	return torrentManager.removeTorrent(*hash, mode);
}
} // namespace Presentation

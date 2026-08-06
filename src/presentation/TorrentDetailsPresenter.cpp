#include "presentation/TorrentDetailsPresenter.hpp"

#include "presentation/UiFormatters.hpp"
#include "utils/TorrentIdentity.hpp"

#include <algorithm>
#include <filesystem>

namespace Presentation
{
TorrentDetailsPresenter::TorrentDetailsPresenter(TorrentManager &torrentManager,
	Utils::SystemUtils::SystemOpener &systemOpener)
	: torrentManager(torrentManager), systemOpener(systemOpener)
{
}

void TorrentDetailsPresenter::setSelectedTorrent(std::optional<lt::info_hash_t> hash)
{
	selectedTorrent_ = std::move(hash);
}

std::optional<lt::torrent_handle> TorrentDetailsPresenter::selectedHandle() const
{
	if (!selectedTorrent_)
		return std::nullopt;
	for (const auto &torrent : torrentManager.getTorrentSnapshot())
	{
		if (torrent.hash == *selectedTorrent_ && torrent.handle.is_valid())
			return torrent.handle;
	}
	return std::nullopt;
}

std::optional<TorrentGeneralDetailsDto> TorrentDetailsPresenter::buildGeneral() const
{
	const auto handle = selectedHandle();
	if (!handle)
		return std::nullopt;
	const auto status = torrentManager.getCachedStatus(*selectedTorrent_);
	if (!status)
		return std::nullopt;

	TorrentGeneralDetailsDto details;
	details.id = Utils::TorrentIdentity::id(*selectedTorrent_);
	details.name = status->name;
	details.stateLabel = UiFormatters::torrentStateToString(
		static_cast<int>(status->state),
		(status->flags & lt::torrent_flags::paused) != lt::torrent_flags_t{},
		status->is_finished);
	details.sizeLabel = UiFormatters::formatBytes(status->total_wanted);
	details.progress = std::clamp(status->progress, 0.0f, 1.0f);
	details.progressLabel = UiFormatters::formatProgress(details.progress);
	details.downloadRateLabel = UiFormatters::formatRate(status->download_payload_rate);
	details.uploadRateLabel = UiFormatters::formatRate(status->upload_payload_rate);
	if (status->state == lt::torrent_status::downloading && status->download_payload_rate > 0)
	{
		const auto remaining = std::max<std::int64_t>(0, status->total_wanted - status->total_wanted_done);
		details.etaLabel = UiFormatters::formatEta(remaining / status->download_payload_rate);
	}
	else
	{
		details.etaLabel = UiFormatters::formatEta(-1);
	}
	details.seedsPeersLabel = std::to_string(status->num_seeds) + " / " + std::to_string(status->num_peers);
	details.downloadedLabel = UiFormatters::formatBytes(status->total_done);
	details.uploadedLabel = UiFormatters::formatBytes(status->all_time_upload);
	details.savePath = status->save_path;
	return details;
}

DetailsState TorrentDetailsPresenter::mapState(TorrentDetailState state)
{
	switch (state)
	{
	case TorrentDetailState::Ready:
		return DetailsState::Ready;
	case TorrentDetailState::Unavailable:
		return DetailsState::Unavailable;
	case TorrentDetailState::Failed:
		return DetailsState::Failed;
	case TorrentDetailState::Loading:
	default:
		return DetailsState::Loading;
	}
}

TorrentDetailsDto TorrentDetailsPresenter::buildSection(DetailsTab tab)
{
	TorrentDetailsDto result;
	if (!selectedTorrent_ || tab == DetailsTab::General || tab == DetailsTab::Settings)
		return result;

	const auto section = tab == DetailsTab::Files
		? TorrentDetailSection::Files
		: tab == DetailsTab::Peers ? TorrentDetailSection::Peers : TorrentDetailSection::Trackers;
	torrentManager.requestDetailsRefresh(*selectedTorrent_, section);
	const auto snapshot = torrentManager.getDetailsSnapshot(*selectedTorrent_, section);
	if (!snapshot)
		return result;

	result.state = mapState(snapshot->state);
	result.revision = snapshot->revision;
	result.message = snapshot->message;
	result.savePath = snapshot->savePath;
	result.truncated = snapshot->truncated;
	for (const auto &file : snapshot->files)
	{
		TorrentFileRowDto row;
		row.index = file.index;
		row.name = file.name;
		row.relativePath = file.relativePath;
		row.sizeBytes = file.size;
		row.downloadedBytes = file.downloaded;
		row.sizeLabel = UiFormatters::formatBytes(file.size);
		row.progress = file.size > 0 ? std::clamp(static_cast<float>(file.downloaded) / static_cast<float>(file.size), 0.0f, 1.0f) : 0.0f;
		row.progressLabel = UiFormatters::formatProgress(row.progress);
		row.priority = file.priority;
		row.previewable = Utils::SystemUtils::isPreviewableFile(file.name);
		result.files.push_back(std::move(row));
	}
	for (const auto &peer : snapshot->peers)
	{
		result.peers.push_back({peer.address, peer.client, peer.flags,
			UiFormatters::formatRate(peer.downloadSpeed), UiFormatters::formatRate(peer.uploadSpeed)});
	}
	for (const auto &tracker : snapshot->trackers)
		result.trackers.push_back({tracker.url, tracker.verified ? "Verified" : "Not Verified", tracker.verified});
	return result;
}

std::optional<TorrentSettingsDto> TorrentDetailsPresenter::buildSettings() const
{
	const auto handle = selectedHandle();
	if (!handle)
		return std::nullopt;
	return TorrentSettingsDto{
		handle->download_limit(),
		handle->upload_limit(),
		(handle->flags() & lt::torrent_flags::sequential_download) != lt::torrent_flags_t{}};
}

Result TorrentDetailsPresenter::setFilePriority(int fileIndex, int priority)
{
	if (!selectedTorrent_)
		return Result::Failure("No torrent is selected", ResultCode::NotFound);
	return torrentManager.setFilePriority(*selectedTorrent_, fileIndex, priority);
}

Result TorrentDetailsPresenter::setSpeedLimits(int downloadBytesPerSecond, int uploadBytesPerSecond)
{
	const auto handle = selectedHandle();
	if (!handle)
		return Result::Failure("No torrent is selected", ResultCode::NotFound);
	if (downloadBytesPerSecond < 0 || uploadBytesPerSecond < 0)
		return Result::Failure("Speed limits cannot be negative", ResultCode::InvalidInput);
	handle->set_download_limit(downloadBytesPerSecond);
	handle->set_upload_limit(uploadBytesPerSecond);
	return Result::Success();
}

Result TorrentDetailsPresenter::setSequentialDownload(bool enabled)
{
	if (!selectedTorrent_)
		return Result::Failure("No torrent is selected", ResultCode::NotFound);
	torrentManager.setSequentialDownload(*selectedTorrent_, enabled);
	return Result::Success();
}

Result TorrentDetailsPresenter::openContainingFolder()
{
	const auto details = buildGeneral();
	if (!details)
		return Result::Failure("Torrent details are not available", ResultCode::NotFound);
	return systemOpener.enqueueExplorer(details->savePath);
}

Result TorrentDetailsPresenter::previewFile(int fileIndex)
{
	if (!selectedTorrent_)
		return Result::Failure("No torrent is selected", ResultCode::NotFound);
	const auto snapshot = torrentManager.getDetailsSnapshot(*selectedTorrent_, TorrentDetailSection::Files);
	if (!snapshot || snapshot->state != TorrentDetailState::Ready)
		return Result::Failure("File details are not available", ResultCode::Unavailable, true);
	const auto found = std::find_if(snapshot->files.begin(), snapshot->files.end(), [fileIndex](const TorrentFileSnapshot &file)
	{
		return file.index == fileIndex;
	});
	if (found == snapshot->files.end())
		return Result::Failure("File is no longer available", ResultCode::NotFound);
	if (!Utils::SystemUtils::isPreviewableFile(found->name))
		return Result::Failure("File type cannot be previewed", ResultCode::InvalidInput);

	const Result sequential = setSequentialDownload(true);
	if (!sequential)
		return sequential;
	const Result priority = setFilePriority(found->index,
		static_cast<int>(static_cast<lt::download_priority_t::underlying_type>(lt::top_priority)));
	if (!priority)
		return priority;
	return systemOpener.enqueuePreview((std::filesystem::path(snapshot->savePath) / found->relativePath).string());
}

Result TorrentDetailsPresenter::previewLargestMediaFile()
{
	if (!selectedTorrent_)
		return Result::Failure("No torrent is selected", ResultCode::NotFound);
	const auto snapshot = torrentManager.getDetailsSnapshot(*selectedTorrent_, TorrentDetailSection::Files);
	if (!snapshot || snapshot->state != TorrentDetailState::Ready)
		return Result::Failure("File details are not available", ResultCode::Unavailable, true);
	const TorrentFileSnapshot *largest = nullptr;
	for (const auto &file : snapshot->files)
	{
		if (Utils::SystemUtils::isPreviewableFile(file.name) && (!largest || file.size > largest->size))
			largest = &file;
	}
	if (!largest)
		return Result::Failure("No previewable media file was found", ResultCode::NotFound);
	return previewFile(largest->index);
}
Result TorrentDetailsPresenter::copyMagnetUri()
{
	const auto handle = selectedHandle();
	if (!handle)
		return Result::Failure("No torrent is selected", ResultCode::NotFound);
	return Utils::SystemUtils::copyToClipboard(lt::make_magnet_uri(*handle));
}
} // namespace Presentation

#pragma once

#include "presentation/UiDtos.hpp"
#include "Result.hpp"
#include "SystemUtils.hpp"
#include "TorrentManager.hpp"

#include <optional>

namespace Presentation
{
enum class DetailsTab
{
	General,
	Files,
	Peers,
	Trackers,
	Settings
};

struct TorrentSettingsDto
{
	int downloadLimitBytes = 0;
	int uploadLimitBytes = 0;
	bool sequentialDownload = false;
};

class TorrentDetailsPresenter
{
public:
	TorrentDetailsPresenter(TorrentManager &torrentManager, Utils::SystemUtils::SystemOpener &systemOpener);

	void setSelectedTorrent(std::optional<lt::info_hash_t> hash);
	const std::optional<lt::info_hash_t> &selectedTorrent() const { return selectedTorrent_; }
	void setSelectedTab(DetailsTab tab) { selectedTab_ = tab; }
	DetailsTab selectedTab() const { return selectedTab_; }

	std::optional<TorrentGeneralDetailsDto> buildGeneral() const;
	TorrentDetailsDto buildSection(DetailsTab tab);
	std::optional<TorrentSettingsDto> buildSettings() const;

	Result setFilePriority(int fileIndex, int priority);
	Result setSpeedLimits(int downloadBytesPerSecond, int uploadBytesPerSecond);
	Result setSequentialDownload(bool enabled);
	Result openContainingFolder();
	Result previewFile(int fileIndex);
	Result previewLargestMediaFile();

private:
	TorrentManager &torrentManager;
	Utils::SystemUtils::SystemOpener &systemOpener;
	std::optional<lt::info_hash_t> selectedTorrent_;
	DetailsTab selectedTab_ = DetailsTab::General;

	std::optional<lt::torrent_handle> selectedHandle() const;
	static DetailsState mapState(TorrentDetailState state);
};
} // namespace Presentation

#pragma once

#include "main-window.h"
#include "presentation/UiDtos.hpp"

#include <memory>
#include <vector>

class DetailsModelAdapter
{
public:
	DetailsModelAdapter();

	void updateFiles(const std::vector<Presentation::TorrentFileRowDto> &rows);
	void updatePeers(const std::vector<Presentation::TorrentPeerRowDto> &rows);
	void updateTrackers(const std::vector<Presentation::TorrentTrackerRowDto> &rows);

	const std::shared_ptr<slint::VectorModel<DetailFileRow>> &filesModel() const { return files_; }
	const std::shared_ptr<slint::VectorModel<DetailPeerRow>> &peersModel() const { return peers_; }
	const std::shared_ptr<slint::VectorModel<DetailTrackerRow>> &trackersModel() const { return trackers_; }

private:
	std::shared_ptr<slint::VectorModel<DetailFileRow>> files_;
	std::shared_ptr<slint::VectorModel<DetailPeerRow>> peers_;
	std::shared_ptr<slint::VectorModel<DetailTrackerRow>> trackers_;
	std::vector<DetailFileRow> fileRows_;
	std::vector<DetailPeerRow> peerRows_;
	std::vector<DetailTrackerRow> trackerRows_;
};

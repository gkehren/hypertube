#include "DetailsModelAdapter.hpp"

#include <algorithm>
#include <string>

namespace
{
template <typename Model, typename Mapper>
void replaceModel(const std::shared_ptr<Model> &model, std::size_t count, Mapper mapper)
{
	while (model->row_count() > 0)
		model->erase(model->row_count() - 1);
	for (std::size_t index = 0; index < count; ++index)
		model->push_back(mapper(index));
}
}

DetailsModelAdapter::DetailsModelAdapter()
	: files_(std::make_shared<slint::VectorModel<DetailFileRow>>()),
	  peers_(std::make_shared<slint::VectorModel<DetailPeerRow>>()),
	  trackers_(std::make_shared<slint::VectorModel<DetailTrackerRow>>())
{
}

void DetailsModelAdapter::updateFiles(const std::vector<Presentation::TorrentFileRowDto> &rows)
{
	replaceModel(files_, rows.size(), [&rows](std::size_t index)
	{
		const auto &row = rows[index];
		return DetailFileRow{row.index, slint::SharedString(row.name), slint::SharedString(row.sizeLabel),
			slint::SharedString(row.progressLabel), slint::SharedString(std::to_string(row.priority)), row.priority, row.previewable};
	});
}

void DetailsModelAdapter::updatePeers(const std::vector<Presentation::TorrentPeerRowDto> &rows)
{
	replaceModel(peers_, rows.size(), [&rows](std::size_t index)
	{
		const auto &row = rows[index];
		return DetailPeerRow{slint::SharedString(row.address), slint::SharedString(row.client),
			slint::SharedString(row.flags), slint::SharedString(row.downloadSpeedLabel),
			slint::SharedString(row.uploadSpeedLabel)};
	});
}

void DetailsModelAdapter::updateTrackers(const std::vector<Presentation::TorrentTrackerRowDto> &rows)
{
	replaceModel(trackers_, rows.size(), [&rows](std::size_t index)
	{
		const auto &row = rows[index];
		return DetailTrackerRow{slint::SharedString(row.url), slint::SharedString(row.statusLabel), row.verified};
	});
}

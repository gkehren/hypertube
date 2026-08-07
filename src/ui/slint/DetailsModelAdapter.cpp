#include "DetailsModelAdapter.hpp"
#include "SlintString.hpp"

#include <string>

namespace
{
bool equal(const DetailFileRow &left, const DetailFileRow &right)
{
	return left.index == right.index && left.name == right.name && left.size_label == right.size_label
		&& left.progress_label == right.progress_label && left.priority_label == right.priority_label
		&& left.priority == right.priority && left.previewable == right.previewable;
}
bool equal(const DetailPeerRow &left, const DetailPeerRow &right)
{
	return left.address == right.address && left.client == right.client && left.flags == right.flags
		&& left.download_speed_label == right.download_speed_label
		&& left.upload_speed_label == right.upload_speed_label;
}
bool equal(const DetailTrackerRow &left, const DetailTrackerRow &right)
{
	return left.url == right.url && left.status_label == right.status_label && left.verified == right.verified;
}

template <typename Model, typename Row, typename Mapper, typename Equal>
void updateModel(const std::shared_ptr<Model> &model, std::vector<Row> &current, std::size_t count,
	Mapper mapper, Equal equalRows)
{
	std::vector<Row> next;
	next.reserve(count);
	for (std::size_t index = 0; index < count; ++index)
		next.push_back(mapper(index));
	if (current.size() == next.size())
	{
		bool unchanged = true;
		for (std::size_t index = 0; index < next.size(); ++index)
			if (!equalRows(current[index], next[index]))
			{
				unchanged = false;
				break;
			}
		if (unchanged)
			return;
		for (std::size_t index = 0; index < next.size(); ++index)
			if (!equalRows(current[index], next[index]))
				model->set_row_data(index, next[index]);
		current = std::move(next);
		return;
	}
	while (model->row_count() > 0)
		model->erase(model->row_count() - 1);
	for (const auto &row : next)
		model->push_back(row);
	current = std::move(next);
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
	updateModel(files_, fileRows_, rows.size(), [&rows](std::size_t index)
	{
		const auto &row = rows[index];
		return DetailFileRow{row.index, SlintUi::toSharedString(row.name), SlintUi::toSharedString(row.sizeLabel),
			SlintUi::toSharedString(row.progressLabel), SlintUi::toSharedString(std::to_string(row.priority)),
			row.priority, row.previewable};
	}, [](const DetailFileRow &left, const DetailFileRow &right) { return equal(left, right); });
}

void DetailsModelAdapter::updatePeers(const std::vector<Presentation::TorrentPeerRowDto> &rows)
{
	updateModel(peers_, peerRows_, rows.size(), [&rows](std::size_t index)
	{
		const auto &row = rows[index];
		return DetailPeerRow{SlintUi::toSharedString(row.address), SlintUi::toSharedString(row.client),
			SlintUi::toSharedString(row.flags), SlintUi::toSharedString(row.downloadSpeedLabel),
			SlintUi::toSharedString(row.uploadSpeedLabel)};
	}, [](const DetailPeerRow &left, const DetailPeerRow &right) { return equal(left, right); });
}

void DetailsModelAdapter::updateTrackers(const std::vector<Presentation::TorrentTrackerRowDto> &rows)
{
	updateModel(trackers_, trackerRows_, rows.size(), [&rows](std::size_t index)
	{
		const auto &row = rows[index];
		return DetailTrackerRow{SlintUi::toSharedString(row.url), SlintUi::toSharedString(row.statusLabel),
			row.verified};
	}, [](const DetailTrackerRow &left, const DetailTrackerRow &right) { return equal(left, right); });
}

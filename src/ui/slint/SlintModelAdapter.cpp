#include "SlintModelAdapter.hpp"
#include "SlintString.hpp"

#include <string>

SlintModelAdapter::SlintModelAdapter()
	: model_(std::make_shared<Model>())
{
}

TorrentRow SlintModelAdapter::toSlintRow(const Presentation::TorrentRowDto &row)
{
	TorrentRow result;
	result.id = SlintUi::toSharedString(row.id);
	result.name = SlintUi::toSharedString(row.name);
	result.state_label = SlintUi::toSharedString(row.stateLabel);
	result.progress_label = SlintUi::toSharedString(row.progressLabel);
	result.progress = row.progress;
	result.size_label = SlintUi::toSharedString(row.sizeLabel);
	result.download_rate_label = SlintUi::toSharedString(row.downloadRateLabel);
	result.upload_rate_label = SlintUi::toSharedString(row.uploadRateLabel);
	result.peers_label = SlintUi::toSharedString(row.peersLabel);
	result.seeds_label = SlintUi::toSharedString(row.seedsLabel);
	result.eta_label = SlintUi::toSharedString(row.etaLabel);
	result.paused = row.paused;
	result.error = row.error;
	result.active = row.active;
	return result;
}

bool SlintModelAdapter::equal(const TorrentRow &left, const TorrentRow &right)
{
	return left.id == right.id && left.name == right.name && left.state_label == right.state_label
		&& left.progress_label == right.progress_label && left.progress == right.progress
		&& left.size_label == right.size_label && left.download_rate_label == right.download_rate_label
		&& left.upload_rate_label == right.upload_rate_label && left.peers_label == right.peers_label
		&& left.seeds_label == right.seeds_label && left.eta_label == right.eta_label
		&& left.paused == right.paused && left.error == right.error && left.active == right.active;
}

void SlintModelAdapter::update(const std::vector<Presentation::TorrentRowDto> &rows)
{
	std::vector<TorrentRow> next;
	next.reserve(rows.size());
	for (const auto &row : rows)
		next.push_back(toSlintRow(row));

	bool sameOrder = rows_.size() == next.size();
	if (sameOrder)
		for (std::size_t index = 0; index < next.size(); ++index)
			if (rows_[index].id != next[index].id)
			{
				sameOrder = false;
				break;
			}
	if (sameOrder)
	{
		for (std::size_t index = 0; index < next.size(); ++index)
			if (!equal(rows_[index], next[index]))
			{
				model_->set_row_data(index, next[index]);
				rows_[index] = next[index];
			}
		return;
	}

	indexById_.clear();
	for (std::size_t index = 0; index < rows_.size(); ++index)
		indexById_.emplace(std::string(rows_[index].id.begin(), rows_[index].id.end()), index);

	while (rows_.size() > next.size())
	{
		const auto index = rows_.size() - 1;
		model_->erase(index);
		rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(index));
	}
	while (rows_.size() < next.size())
	{
		const auto index = rows_.size();
		model_->push_back(next[index]);
		rows_.push_back(next[index]);
	}
	for (std::size_t index = 0; index < next.size(); ++index)
	{
		if (!equal(rows_[index], next[index]))
			model_->set_row_data(index, next[index]);
		rows_[index] = next[index];
	}

	indexById_.clear();
	for (std::size_t index = 0; index < rows_.size(); ++index)
		indexById_.emplace(std::string(rows_[index].id.begin(), rows_[index].id.end()), index);
}

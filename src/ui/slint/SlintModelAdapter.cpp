#include "SlintModelAdapter.hpp"

#include <algorithm>
#include <utility>

SlintModelAdapter::SlintModelAdapter()
	: model_(std::make_shared<Model>())
{
}

TorrentRow SlintModelAdapter::toSlintRow(const Presentation::TorrentRowDto &row)
{
	TorrentRow result;
	result.id = slint::SharedString(row.id);
	result.name = slint::SharedString(row.name);
	result.state_label = slint::SharedString(row.stateLabel);
	result.progress_label = slint::SharedString(row.progressLabel);
	result.progress = row.progress;
	result.size_label = slint::SharedString(row.sizeLabel);
	result.download_rate_label = slint::SharedString(row.downloadRateLabel);
	result.upload_rate_label = slint::SharedString(row.uploadRateLabel);
	result.peers_label = slint::SharedString(row.peersLabel);
	result.seeds_label = slint::SharedString(row.seedsLabel);
	result.eta_label = slint::SharedString(row.etaLabel);
	result.paused = row.paused;
	result.error = row.error;
	result.active = row.active;
	return result;
}

bool SlintModelAdapter::equal(const TorrentRow &left, const TorrentRow &right)
{
	return left.id == right.id && left.name == right.name && left.state_label == right.state_label &&
		left.progress_label == right.progress_label && left.progress == right.progress &&
		left.size_label == right.size_label &&
		left.download_rate_label == right.download_rate_label && left.upload_rate_label == right.upload_rate_label &&
		left.peers_label == right.peers_label && left.seeds_label == right.seeds_label &&
		left.eta_label == right.eta_label && left.paused == right.paused && left.error == right.error &&
		left.active == right.active;
}

void SlintModelAdapter::update(const std::vector<Presentation::TorrentRowDto> &rows)
{
	std::vector<TorrentRow> next;
	next.reserve(rows.size());
	for (const auto &row : rows)
		next.push_back(toSlintRow(row));

	// Reconcile by the stable torrent ID. Refreshes commonly change order as
	// sorting or filtering changes, so positional replacement would make the
	// model report every row as changed and could briefly display the wrong
	// action target.
	for (std::size_t index = 0; index < next.size(); ++index)
	{
		auto existing = std::find_if(rows_.begin() + static_cast<std::ptrdiff_t>(index), rows_.end(),
			[&next, index](const TorrentRow &row) { return row.id == next[index].id; });

		if (existing == rows_.end())
		{
			model_->insert(index, next[index]);
			rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(index), next[index]);
			continue;
		}

		const auto existingIndex = static_cast<std::size_t>(std::distance(rows_.begin(), existing));
		if (existingIndex != index)
		{
			const auto preserved = *existing;
			model_->erase(existingIndex);
			rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(existingIndex));
			model_->insert(index, preserved);
			rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(index), preserved);
		}

		if (!equal(rows_[index], next[index]))
		{
			model_->set_row_data(index, next[index]);
			rows_[index] = next[index];
		}
	}

	while (rows_.size() > next.size())
	{
		const auto index = rows_.size() - 1;
		model_->erase(index);
		rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(index));
	}
}

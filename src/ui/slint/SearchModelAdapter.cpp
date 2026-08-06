#include "SearchModelAdapter.hpp"
#include "SlintString.hpp"

#include <string>

namespace
{
SearchResultRow toSlintRow(const Presentation::SearchResultDto &source)
{
	return SearchResultRow{
		SlintUi::toSharedString(source.id),
		SlintUi::toSharedString(source.name),
		SlintUi::toSharedString(source.sizeLabel),
		SlintUi::toSharedString(source.seedersLabel),
		SlintUi::toSharedString(source.leechersLabel),
		SlintUi::toSharedString(source.category),
		SlintUi::toSharedString(source.createdLabel),
		source.favorite,
		!source.magnetUri.empty()};
}

bool equal(const SearchResultRow &left, const SearchResultRow &right)
{
	return left.id == right.id && left.name == right.name && left.size_label == right.size_label
		&& left.seeders_label == right.seeders_label && left.leechers_label == right.leechers_label
		&& left.category == right.category && left.published_label == right.published_label
		&& left.favorite == right.favorite && left.magnet_available == right.magnet_available;
}
}

SearchModelAdapter::SearchModelAdapter()
	: model_(std::make_shared<slint::VectorModel<SearchResultRow>>())
{
}

void SearchModelAdapter::update(const std::vector<Presentation::SearchResultDto> &rows)
{
	std::vector<SearchResultRow> next;
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

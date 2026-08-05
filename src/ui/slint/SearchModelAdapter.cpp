#include "SearchModelAdapter.hpp"

#include <algorithm>
#include <utility>

namespace
{
SearchResultRow toSlintRow(const Presentation::SearchResultDto &source)
{
	return SearchResultRow{
		slint::SharedString(source.id),
		slint::SharedString(source.name),
		slint::SharedString(source.sizeLabel),
		slint::SharedString(source.seedersLabel),
		slint::SharedString(source.leechersLabel),
		slint::SharedString(source.category),
		slint::SharedString(source.createdLabel),
		source.favorite,
		!source.magnetUri.empty()};
}

bool equal(const SearchResultRow &left, const SearchResultRow &right)
{
	return left.id == right.id && left.name == right.name && left.size_label == right.size_label &&
		left.seeders_label == right.seeders_label && left.leechers_label == right.leechers_label &&
		left.category == right.category && left.published_label == right.published_label &&
		left.favorite == right.favorite && left.magnet_available == right.magnet_available;
}
} // namespace

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

	std::vector<SearchResultRow> current;
	current.reserve(static_cast<std::size_t>(model_->row_count()));
	for (int index = 0; index < model_->row_count(); ++index)
		current.push_back(*model_->row_data(index));

	for (std::size_t index = 0; index < next.size(); ++index)
	{
		auto existing = std::find_if(current.begin() + static_cast<std::ptrdiff_t>(index), current.end(),
			[&next, index](const SearchResultRow &row) { return row.id == next[index].id; });
		if (existing == current.end())
		{
			model_->insert(index, next[index]);
			current.insert(current.begin() + static_cast<std::ptrdiff_t>(index), next[index]);
			continue;
		}

		const auto existingIndex = static_cast<std::size_t>(std::distance(current.begin(), existing));
		if (existingIndex != index)
		{
			const auto preserved = *existing;
			model_->erase(existingIndex);
			current.erase(current.begin() + static_cast<std::ptrdiff_t>(existingIndex));
			model_->insert(index, preserved);
			current.insert(current.begin() + static_cast<std::ptrdiff_t>(index), preserved);
		}

		if (!equal(current[index], next[index]))
		{
			model_->set_row_data(index, next[index]);
			current[index] = next[index];
		}
	}
	while (current.size() > next.size())
	{
		const auto index = current.size() - 1;
		model_->erase(index);
		current.erase(current.begin() + static_cast<std::ptrdiff_t>(index));
	}
}

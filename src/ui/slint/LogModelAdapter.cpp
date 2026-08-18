#include "LogModelAdapter.hpp"
#include "SlintString.hpp"

LogModelAdapter::LogModelAdapter()
	: model_(std::make_shared<slint::VectorModel<LogRow>>())
{
}

bool LogModelAdapter::equal(const LogRow &left, const LogRow &right)
{
	return left.timestamp == right.timestamp && left.level == right.level
		&& left.category == right.category && left.message == right.message
		&& left.severity == right.severity;
}

void LogModelAdapter::update(const std::vector<Presentation::LogRowDto> &rows)
{
	std::vector<LogRow> next;
	next.reserve(rows.size());
	for (const auto &row : rows)
	{
		next.push_back(LogRow{
			SlintUi::toSharedString(row.timestamp),
			SlintUi::toSharedString(row.level),
			SlintUi::toSharedString(row.category),
			SlintUi::toSharedString(row.message),
			row.severity
		});
	}

	if (rows_.size() == next.size())
	{
		bool identical = true;
		for (std::size_t i = 0; i < next.size(); ++i)
		{
			if (!equal(rows_[i], next[i]))
			{
				identical = false;
				break;
			}
		}
		if (identical)
			return;
	}

	// Efficient incremental append if next extends rows_
	if (next.size() > rows_.size())
	{
		bool prefixMatch = true;
		for (std::size_t i = 0; i < rows_.size(); ++i)
		{
			if (!equal(rows_[i], next[i]))
			{
				prefixMatch = false;
				break;
			}
		}
		if (prefixMatch)
		{
			for (std::size_t i = rows_.size(); i < next.size(); ++i)
			{
				model_->push_back(next[i]);
			}
			rows_ = std::move(next);
			return;
		}
	}

	// Full reset if cleared or re-ordered
	while (model_->row_count() > 0)
		model_->erase(model_->row_count() - 1);
	for (const auto &row : next)
		model_->push_back(row);

	rows_ = std::move(next);
}

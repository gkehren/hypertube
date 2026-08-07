#include "LogModelAdapter.hpp"
#include "SlintString.hpp"

LogModelAdapter::LogModelAdapter()
	: model_(std::make_shared<slint::VectorModel<LogRow>>())
{
}

void LogModelAdapter::update(const std::vector<Presentation::LogRowDto> &rows)
{
	while (model_->row_count() > 0)
		model_->erase(model_->row_count() - 1);
	for (const auto &row : rows)
		model_->push_back(LogRow{SlintUi::toSharedString(row.timestamp), SlintUi::toSharedString(row.level),
			SlintUi::toSharedString(row.category), SlintUi::toSharedString(row.message), row.severity});
}

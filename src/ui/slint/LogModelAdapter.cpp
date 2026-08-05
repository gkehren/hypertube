#include "LogModelAdapter.hpp"

LogModelAdapter::LogModelAdapter()
	: model_(std::make_shared<slint::VectorModel<LogRow>>())
{
}

void LogModelAdapter::update(const std::vector<Presentation::LogRowDto> &rows)
{
	while (model_->row_count() > 0)
		model_->erase(model_->row_count() - 1);
	for (const auto &row : rows)
		model_->push_back(LogRow{slint::SharedString(row.timestamp), slint::SharedString(row.level),
			slint::SharedString(row.category), slint::SharedString(row.message), row.severity});
}

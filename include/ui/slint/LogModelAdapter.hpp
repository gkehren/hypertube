#pragma once

#include "main-window.h"
#include "presentation/UiDtos.hpp"

#include <memory>
#include <vector>

class LogModelAdapter
{
public:
	LogModelAdapter();
	void update(const std::vector<Presentation::LogRowDto> &rows);
	const std::shared_ptr<slint::VectorModel<LogRow>> &model() const { return model_; }
	std::size_t size() const { return rows_.size(); }

private:
	static bool equal(const LogRow &left, const LogRow &right);
	std::shared_ptr<slint::VectorModel<LogRow>> model_;
	std::vector<LogRow> rows_;
};

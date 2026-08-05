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

private:
	std::shared_ptr<slint::VectorModel<LogRow>> model_;
};

#pragma once

#include "main-window.h"
#include "presentation/UiDtos.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class SearchModelAdapter
{
public:
	SearchModelAdapter();

	void update(const std::vector<Presentation::SearchResultDto> &rows);
	const std::shared_ptr<slint::VectorModel<SearchResultRow>> &model() const { return model_; }

private:
	std::shared_ptr<slint::VectorModel<SearchResultRow>> model_;
	std::vector<SearchResultRow> rows_;
	std::unordered_map<std::string, std::size_t> indexById_;
};

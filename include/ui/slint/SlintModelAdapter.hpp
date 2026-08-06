#pragma once

#include "main-window.h"
#include "presentation/UiDtos.hpp"

#include <slint.h>

#include <memory>
#include <unordered_map>
#include <vector>

class SlintModelAdapter
{
public:
	struct UpdateStats { std::size_t changed = 0; std::size_t inserted = 0; std::size_t removed = 0; };
	using Model = slint::VectorModel<TorrentRow>;

	SlintModelAdapter();

	std::shared_ptr<Model> model() const { return model_; }
	void update(const std::vector<Presentation::TorrentRowDto> &rows);
	UpdateStats lastUpdateStats() const { return lastUpdateStats_; }

private:
	static TorrentRow toSlintRow(const Presentation::TorrentRowDto &row);
	static bool equal(const TorrentRow &left, const TorrentRow &right);

	std::shared_ptr<Model> model_;
	std::vector<TorrentRow> rows_;
	std::unordered_map<std::string, std::size_t> indexById_;
	UpdateStats lastUpdateStats_;
};

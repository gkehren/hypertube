#include "presentation/UiDtos.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

std::vector<Presentation::TorrentRowDto> makeRows(std::size_t count)
{
	std::vector<Presentation::TorrentRowDto> rows;
	rows.reserve(count);
	for (std::size_t index = 0; index < count; ++index)
	{
		Presentation::TorrentRowDto row;
		row.id = "v1:" + std::to_string(index);
		row.name = "Synthetic torrent " + std::to_string(index);
		row.stateLabel = index % 3 == 0 ? "Downloading" : "Seeding";
		row.progress = static_cast<float>(index % 100) / 100.0f;
		row.downloadRateBytes = static_cast<std::int64_t>(index * 17);
		row.active = index % 4 != 0;
		row.state = row.stateLabel == "Downloading" ? Presentation::TorrentUiState::Downloading
			: Presentation::TorrentUiState::Seeding;
		rows.push_back(std::move(row));
	}
	return rows;
}

template <typename Function>
double measure(Function &&function, int iterations = 10)
{
	const auto start = Clock::now();
	for (int iteration = 0; iteration < iterations; ++iteration)
		function();
	return std::chrono::duration<double, std::milli>(Clock::now() - start).count() / iterations;
}
} // namespace

int main()
{
	std::cout << "rows,no_op_ms,status_revision_ms,filter_ms,sort_ms\n";
	for (const std::size_t count : {100U, 1000U, 10000U})
	{
		auto rows = makeRows(count);
		volatile std::size_t observed = 0;
		const double noOp = measure([&] {
			observed += rows.size();
		}, 100);
		const double rebuild = measure([&] {
			auto rebuilt = makeRows(count);
			observed += rebuilt.size();
		});
		const double filter = measure([&] {
			auto filtered = rows;
			filtered.erase(std::remove_if(filtered.begin(), filtered.end(), [](const auto &row) {
				return row.stateLabel != "Downloading";
			}), filtered.end());
			observed += filtered.size();
		});
		const double sort = measure([&] {
			auto sorted = rows;
			std::stable_sort(sorted.begin(), sorted.end(), [](const auto &left, const auto &right) {
				return left.downloadRateBytes < right.downloadRateBytes;
			});
			observed += sorted.size();
		});
		std::cout << count << ',' << std::fixed << std::setprecision(3) << noOp << ',' << rebuild << ','
			<< filter << ',' << sort << '\n';
	}
	return 0;
}

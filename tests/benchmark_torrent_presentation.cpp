#include "presentation/TorrentListPresenter.hpp"
#include "app/TorrentManager.hpp"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

void addTorrents(TorrentManager &manager, std::size_t start, std::size_t target)
{
	for (std::size_t index = start; index < target; ++index)
	{
		char hexBuf[41];
		std::snprintf(hexBuf, sizeof(hexBuf), "%040zx", index + 1);
		std::string magnet = std::string("magnet:?xt=urn:btih:") + hexBuf + "&dn=SyntheticTorrent" + std::to_string(index);
		manager.addMagnetTorrent(magnet, "/tmp/downloads");
	}
	manager.refreshStatusCache();
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

	TorrentManager manager;
	std::size_t currentCount = 0;

	for (const std::size_t targetCount : {100U, 1000U, 10000U})
	{
		addTorrents(manager, currentCount, targetCount);
		currentCount = targetCount;

		Presentation::TorrentListPresenter presenter(manager);
		// Warmup build
		(void)presenter.buildRows();

		volatile std::size_t observed = 0;
		const double noOp = measure([&] {
			const auto rows = presenter.buildRows();
			observed += rows.size();
		}, 50);

		const int statusIterations = targetCount >= 10000U ? 2 : 10;
		const double statusRebuild = measure([&] {
			manager.refreshStatusCache();
			const auto rows = presenter.buildRows();
			observed += rows.size();
		}, statusIterations);

		int toggleFilter = 0;
		const double filter = measure([&] {
			toggleFilter = (toggleFilter + 1) % 2;
			presenter.setTextFilter(toggleFilter ? "Torrent1" : "");
			const auto rows = presenter.buildRows();
			observed += rows.size();
		}, 5);

		int toggleSort = 0;
		const double sort = measure([&] {
			toggleSort = (toggleSort + 1) % 2;
			presenter.setSort(toggleSort ? Presentation::TorrentSortField::Name : Presentation::TorrentSortField::Queue, true);
			const auto rows = presenter.buildRows();
			observed += rows.size();
		}, 5);

		std::cout << targetCount << ',' << std::fixed << std::setprecision(3) << noOp << ',' << statusRebuild << ','
			<< filter << ',' << sort << '\n';
	}
	return 0;
}

#include <gtest/gtest.h>

#include "presentation/SearchPresenter.hpp"
#include "presentation/TorrentListPresenter.hpp"
#include "presentation/UiFormatters.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
TEST(UiFormattersTest, FormatsSharedValues)
{
	EXPECT_EQ(Presentation::UiFormatters::formatBytes(1536), "1.5 KB");
	EXPECT_EQ(Presentation::UiFormatters::formatRate(1024), "1 KB/s");
	EXPECT_EQ(Presentation::UiFormatters::formatProgress(0.375f), "37.5%");
	EXPECT_EQ(Presentation::UiFormatters::formatEta(125), "2 minutes");
	EXPECT_EQ(Presentation::UiFormatters::formatEta(-1), "N/A");
}

TEST(UiFormattersTest, FormatsRatiosAndTimestamps)
{
	EXPECT_EQ(Presentation::UiFormatters::formatRatio(10, 4), "2.5");
	EXPECT_EQ(Presentation::UiFormatters::formatRatio(2, 0), "∞");
	EXPECT_EQ(Presentation::UiFormatters::formatRatio(0, 0), "-");
	EXPECT_EQ(Presentation::UiFormatters::formatUnixDate(0), "N/A");
	EXPECT_EQ(Presentation::UiFormatters::formatUnixDate(-1), "Invalid TS");
}

TEST(UiFormattersTest, MapsLibtorrentStateAtTheBoundary)
{
	EXPECT_EQ(Presentation::UiFormatters::torrentStateToString(3, false, false), "Downloading");
	EXPECT_EQ(Presentation::UiFormatters::torrentStateToString(5, false, false), "Seeding");
	EXPECT_EQ(Presentation::UiFormatters::torrentStateToString(3, true, false), "Paused");
	EXPECT_EQ(Presentation::UiFormatters::torrentStateToString(3, false, true), "Finished");
}

TEST(TorrentListPresenterTest, EmptyManagerProducesStableEmptyModels)
{
	TorrentManager manager;
	Presentation::TorrentListPresenter presenter(manager);
	presenter.setSelectedId("missing");

	EXPECT_TRUE(presenter.buildRows().empty());
	EXPECT_TRUE(presenter.selectedId().empty());
	const auto categories = presenter.buildCategories();
	ASSERT_EQ(categories.size(), 7U);
	for (const auto &category : categories)
		EXPECT_EQ(category.count, 0);
}

TEST(TorrentListPresenterTest, ResolvesSelectionOutsideTheVisibleFilter)
{
	const auto testDirectory = std::filesystem::temp_directory_path()
		/ ("hypertube-presenter-selection-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(testDirectory / "downloads");
	const auto torrentPath = testDirectory / "fixture.torrent";
	std::string content = "d4:infod6:lengthi1e4:name7:fixture12:piece lengthi16384e6:pieces20:";
	content.append(20, '\0');
	content += "ee";
	{
		std::ofstream file(torrentPath, std::ios::binary);
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	TorrentManager manager;
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), (testDirectory / "downloads").string()));
	manager.requestStatusRefresh();
	std::optional<lt::torrent_status> status;
	for (int attempt = 0; attempt < 100 && !status; ++attempt)
	{
		status = manager.getCachedStatus(manager.getTorrentSnapshot().front().hash);
		if (!status)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ASSERT_TRUE(status);

	Presentation::TorrentListPresenter presenter(manager);
	const auto visibleRows = presenter.buildRows();
	ASSERT_EQ(visibleRows.size(), 1U);
	const std::string id = visibleRows.front().id;

	presenter.setTextFilter("does-not-match");
	EXPECT_TRUE(presenter.buildRows().empty());
	const auto resolved = presenter.findRowById(id);
	ASSERT_TRUE(resolved);
	EXPECT_EQ(resolved->id, id);
	EXPECT_EQ(resolved->name, "fixture");

	std::error_code error;
	std::filesystem::remove_all(testDirectory, error);
}

TEST(SearchPresenterTest, RejectsEmptyQueriesWithoutStartingWork)
{
	SearchEngine engine;
	Presentation::SearchPresenter presenter(engine);
	const Result result = presenter.startSearch("");
	EXPECT_FALSE(result);
	EXPECT_EQ(result.code, ResultCode::InvalidInput);
	EXPECT_EQ(presenter.state(), Presentation::SearchState::Idle);
}
} // namespace

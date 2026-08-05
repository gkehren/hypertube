#include <gtest/gtest.h>

#include "presentation/SearchPresenter.hpp"
#include "presentation/TorrentListPresenter.hpp"
#include "presentation/UiFormatters.hpp"

#include <chrono>

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

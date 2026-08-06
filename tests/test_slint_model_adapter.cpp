#include <gtest/gtest.h>

#include "SearchModelAdapter.hpp"
#include "SlintModelAdapter.hpp"
#include "DetailsModelAdapter.hpp"

namespace
{
Presentation::TorrentRowDto torrent(std::string id, std::string name)
{
	Presentation::TorrentRowDto row;
	row.id = std::move(id);
	row.name = std::move(name);
	row.stateLabel = "Downloading";
	row.progressLabel = "10%";
	row.sizeLabel = "1 KB";
	return row;
}

Presentation::SearchResultDto search(std::string id, std::string name)
{
	Presentation::SearchResultDto row;
	row.id = std::move(id);
	row.name = std::move(name);
	row.sizeLabel = "1 KB";
	row.seedersLabel = "2";
	row.leechersLabel = "1";
	row.category = "video";
	row.createdLabel = "today";
	return row;
}

std::string stringValue(const slint::SharedString &value)
{
	return {value.begin(), value.end()};
}

TEST(SlintModelAdapterTest, ReconcilesTorrentRowsByStableId)
{
	SlintModelAdapter adapter;
	adapter.update({torrent("a", "Alpha"), torrent("b", "Beta")});
	adapter.update({torrent("b", "Beta renamed"), torrent("c", "Gamma")});

	const auto model = adapter.model();
	ASSERT_EQ(model->row_count(), 2U);
	ASSERT_TRUE(model->row_data(0).has_value());
	ASSERT_TRUE(model->row_data(1).has_value());
	EXPECT_EQ(stringValue(model->row_data(0)->id), "b");
	EXPECT_EQ(stringValue(model->row_data(0)->name), "Beta renamed");
	EXPECT_EQ(stringValue(model->row_data(1)->id), "c");

	adapter.update({torrent("c", "Gamma")});
	ASSERT_EQ(model->row_count(), 1U);
	EXPECT_EQ(stringValue(model->row_data(0)->id), "c");
}

TEST(SlintModelAdapterTest, ReplacesMalformedTorrentTextBeforeCrossingSlintBoundary)
{
	SlintModelAdapter adapter;
	adapter.update({torrent("id", std::string("Bad\xFFName", 8))});

	const auto row = adapter.model()->row_data(0);
	ASSERT_TRUE(row.has_value());
	EXPECT_EQ(stringValue(row->name), "Bad\xEF\xBF\xBDName");
}

TEST(SlintModelAdapterTest, ReconcilesSearchRowsByStableId)
{
	SearchModelAdapter adapter;
	auto first = search("a", "Alpha");
	first.favorite = true;
	adapter.update({first, search("b", "Beta")});
	auto changed = search("b", "Beta renamed");
	changed.magnetUri = "magnet:?xt=urn:btih:b";
	auto retained = search("a", "Alpha");
	retained.favorite = true;
	adapter.update({changed, retained});

	const auto model = adapter.model();
	ASSERT_EQ(model->row_count(), 2U);
	EXPECT_EQ(stringValue(model->row_data(0)->id), "b");
	EXPECT_EQ(stringValue(model->row_data(0)->name), "Beta renamed");
	EXPECT_TRUE(model->row_data(0)->magnet_available);
	EXPECT_EQ(stringValue(model->row_data(1)->id), "a");
	EXPECT_TRUE(model->row_data(1)->favorite);
}

TEST(SlintModelAdapterTest, HandlesTenThousandRowsAndAStableRefresh)
{
	SlintModelAdapter adapter;
	std::vector<Presentation::TorrentRowDto> rows;
	rows.reserve(10000);
	for (int index = 0; index < 10000; ++index)
		rows.push_back(torrent(std::to_string(index), "Torrent " + std::to_string(index)));
	adapter.update(rows);
	ASSERT_EQ(adapter.model()->row_count(), 10000U);

	rows[7777].name = "Changed";
	adapter.update(rows);
	ASSERT_EQ(adapter.model()->row_count(), 10000U);
	EXPECT_EQ(stringValue(adapter.model()->row_data(7777)->name), "Changed");
}

TEST(SlintModelAdapterTest, HandlesTenThousandSearchRows)
{
	SearchModelAdapter adapter;
	std::vector<Presentation::SearchResultDto> rows;
	rows.reserve(10000);
	for (int index = 0; index < 10000; ++index)
		rows.push_back(search(std::to_string(index), "Result " + std::to_string(index)));
	adapter.update(rows);
	ASSERT_EQ(adapter.model()->row_count(), 10000U);
	rows[5000].name = "Changed";
	adapter.update(rows);
	EXPECT_EQ(stringValue(adapter.model()->row_data(5000)->name), "Changed");
}

TEST(SlintModelAdapterTest, KeepsDetailsModelIdentityWhenRowsAreUnchanged)
{
	DetailsModelAdapter adapter;
	std::vector<Presentation::TorrentFileRowDto> rows{{
		.index = 0,
		.name = "video.mkv",
		.sizeLabel = "1 KB",
		.progressLabel = "100%",
		.priority = 4,
		.previewable = true,
	}};

	const auto model = adapter.filesModel();
	adapter.updateFiles(rows);
	ASSERT_EQ(model->row_count(), 1U);
	auto first = model->row_data(0);
	ASSERT_TRUE(first.has_value());

	adapter.updateFiles(rows);
	EXPECT_EQ(adapter.filesModel().get(), model.get());
	ASSERT_TRUE(model->row_data(0).has_value());
	EXPECT_EQ(model->row_data(0)->index, first->index);

	rows.front().priority = 7;
	adapter.updateFiles(rows);
	ASSERT_TRUE(model->row_data(0).has_value());
	EXPECT_EQ(model->row_data(0)->priority, 7);
}
} // namespace

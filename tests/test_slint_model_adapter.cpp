#include <gtest/gtest.h>

#include "SearchModelAdapter.hpp"
#include "SlintModelAdapter.hpp"

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
} // namespace

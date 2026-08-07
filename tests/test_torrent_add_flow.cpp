#include <gtest/gtest.h>

#include "presentation/TorrentAddFlow.hpp"

TEST(TorrentAddFlowTest, CancellationClearsTheEntireRequest)
{
	TorrentAddFlow flow;
	TorrentSearchResult result;
	result.infoHash = "0123456789012345678901234567890123456789";
	result.magnetUri = "magnet:?xt=urn:btih:0123456789012345678901234567890123456789";
	flow.beginSearchResult(result);
	ASSERT_TRUE(flow.needsDestinationDialog());
	flow.markDestinationDialogOpened();
	flow.cancel();
	EXPECT_FALSE(flow.hasPendingRequest());
	EXPECT_FALSE(flow.needsDestinationDialog());
}

TEST(TorrentAddFlowTest, DialogIsOpenedOnlyOncePerRequest)
{
	TorrentAddFlow flow;
	flow.beginFile("fixture.torrent");
	ASSERT_TRUE(flow.needsDestinationDialog());
	flow.markDestinationDialogOpened();
	EXPECT_FALSE(flow.needsDestinationDialog());
	ASSERT_TRUE(flow.hasPendingRequest());
	const auto request = flow.take();
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->source, TorrentAddSource::TorrentFile);
	EXPECT_EQ(request->value, "fixture.torrent");
	EXPECT_FALSE(flow.hasPendingRequest());
}

TEST(TorrentAddFlowTest, NewRequestCannotReuseOldSearchSelection)
{
	TorrentAddFlow flow;
	TorrentSearchResult result;
	result.magnetUri = "magnet:?xt=urn:btih:old";
	flow.beginSearchResult(result);
	flow.cancel();
	flow.beginMagnet("magnet:?xt=urn:btih:new");
	const auto request = flow.take();
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->source, TorrentAddSource::Magnet);
	EXPECT_EQ(request->value, "magnet:?xt=urn:btih:new");
}

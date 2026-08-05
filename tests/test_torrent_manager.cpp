#include <gtest/gtest.h>

#include "TorrentManager.hpp"
#include "ConfigManager.hpp"

#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
class TorrentManagerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		testDirectory = std::filesystem::temp_directory_path() / ("hypertube-torrent-test-" + std::to_string(testCounter++));
		std::filesystem::create_directories(testDirectory / "downloads");
	}

	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(testDirectory, error);
	}

	std::filesystem::path writeTorrentFile()
	{
		const auto path = testDirectory / "fixture.torrent";
		std::string content = "d4:infod6:lengthi1e4:name7:fixture12:piece lengthi16384e6:pieces20:";
		content.append(20, '\0');
		content += "ee";
		std::ofstream file(path, std::ios::binary);
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
		return path;
	}

	std::filesystem::path testDirectory;
	static inline int testCounter = 0;
};
}

TEST_F(TorrentManagerTest, RejectsDuplicateWithoutLeavingOrphanedTorrent)
{
	TorrentManager manager;
	const auto torrentPath = writeTorrentFile();
	const auto downloadPath = testDirectory / "downloads";

	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
	Result duplicate = manager.addTorrent(torrentPath.string(), downloadPath.string());
	EXPECT_FALSE(duplicate);
	EXPECT_EQ(duplicate.code, ResultCode::Duplicate);
	ASSERT_EQ(manager.getTorrentSnapshot().size(), 1u);

	const auto hash = manager.getTorrentSnapshot().front().hash;
	ASSERT_TRUE(manager.removeTorrent(hash, TorrentRemovalMode::KeepAllFiles));
	EXPECT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
}

TEST_F(TorrentManagerTest, RejectsInvalidInputsBeforeAdding)
{
	TorrentManager manager;
	Result missing = manager.addTorrent((testDirectory / "missing.torrent").string(), (testDirectory / "downloads").string());
	EXPECT_FALSE(missing);
	EXPECT_EQ(missing.code, ResultCode::InvalidInput);

	Result invalidMagnet = manager.addMagnetTorrent("https://example.invalid/not-a-magnet", (testDirectory / "downloads").string());
	EXPECT_FALSE(invalidMagnet);
	EXPECT_EQ(invalidMagnet.code, ResultCode::InvalidInput);
	EXPECT_TRUE(manager.getTorrentSnapshot().empty());
}

TEST_F(TorrentManagerTest, SupportsV2MagnetsAndDetectsDuplicates)
{
	TorrentManager manager;
	const std::string magnet = "magnet:?xt=urn:btmh:12200123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	const auto downloadPath = testDirectory / "downloads";

	ASSERT_TRUE(manager.addMagnetTorrent(magnet, downloadPath.string()));
	const auto snapshot = manager.getTorrentSnapshot();
	ASSERT_EQ(snapshot.size(), 1u);
	EXPECT_TRUE(snapshot.front().hash.has_v2());

	Result duplicate = manager.addMagnetTorrent(magnet, downloadPath.string());
	EXPECT_FALSE(duplicate);
	EXPECT_EQ(duplicate.code, ResultCode::Duplicate);
}

TEST_F(TorrentManagerTest, FastResumeSnapshotCanRestoreTorrent)
{
	const auto torrentPath = writeTorrentFile();
	const auto downloadPath = testDirectory / "downloads";
	TorrentConfigData persisted;
	{
		TorrentManager manager;
		ASSERT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
		std::vector<ManagedTorrent> snapshot;
		ASSERT_TRUE(manager.getPersistenceSnapshot(snapshot, std::chrono::seconds(2)));
		ASSERT_EQ(snapshot.size(), 1u);
		ASSERT_FALSE(snapshot.front().resumeData.empty());
		persisted.magnetUri = lt::make_magnet_uri(snapshot.front().handle);
		persisted.savePath = downloadPath.string();
		persisted.torrentFilePath = torrentPath.string();
		persisted.resumeData = snapshot.front().resumeData;
	}

	TorrentManager restored;
	restored.addTorrentsFromConfig({persisted});
	ASSERT_EQ(restored.getTorrentSnapshot().size(), 1u);
}

TEST_F(TorrentManagerTest, CollectsFileDetailsOffTheCallingThread)
{
	TorrentManager manager;
	const auto torrentPath = writeTorrentFile();
	const auto downloadPath = testDirectory / "downloads";
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
	const auto hash = manager.getTorrentSnapshot().front().hash;
	manager.requestDetailsRefresh(hash, TorrentDetailSection::Files);

	std::shared_ptr<const TorrentDetailsSnapshot> details;
	for (int attempt = 0; attempt < 100 && !details; ++attempt)
	{
		details = manager.getDetailsSnapshot(hash, TorrentDetailSection::Files);
		if (!details)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ASSERT_TRUE(details);
	EXPECT_EQ(details->state, TorrentDetailState::Ready);
	ASSERT_EQ(details->files.size(), 1u);
	EXPECT_EQ(details->files.front().name, "fixture");
	EXPECT_EQ(details->files.front().size, 1);
}

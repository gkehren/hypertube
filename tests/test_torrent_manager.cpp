#include <gtest/gtest.h>

#include "TorrentManager.hpp"
#include "ConfigManager.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <thread>

namespace
{
std::filesystem::path makeUniqueTestDirectory()
{
	const auto base = std::filesystem::temp_directory_path();
	std::random_device random;

	for (int attempt = 0; attempt < 100; ++attempt)
	{
		const auto candidate = base / ("hypertube-torrent-test-" +
			std::to_string(random()) + "-" + std::to_string(random()));
		std::error_code error;
		if (std::filesystem::create_directory(candidate, error))
			return candidate;
	}

	throw std::runtime_error("Unable to create unique torrent test directory");
}

class TorrentManagerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		testDirectory = makeUniqueTestDirectory();
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
	EXPECT_GT(details->revision, 0u);
	ASSERT_EQ(details->files.size(), 1u);
	EXPECT_EQ(details->files.front().name, "fixture");
	EXPECT_EQ(details->files.front().size, 1);
}

TEST_F(TorrentManagerTest, CollectionRevisionChangesOnlyForSuccessfulMembershipChanges)
{
	TorrentManager manager;
	const auto initial = manager.getTorrentCollectionRevision();
	const auto torrentPath = writeTorrentFile();
	const auto downloadPath = testDirectory / "downloads";
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
	const auto added = manager.getTorrentCollectionRevision();
	EXPECT_GT(added, initial);
	EXPECT_FALSE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
	EXPECT_EQ(manager.getTorrentCollectionRevision(), added);
	const auto hash = manager.getTorrentSnapshot().front().hash;
	ASSERT_TRUE(manager.removeTorrent(hash, TorrentRemovalMode::KeepAllFiles));
	EXPECT_GT(manager.getTorrentCollectionRevision(), added);
}

TEST_F(TorrentManagerTest, RefreshesStatusCacheWhenRequested)
{
	TorrentManager manager;
	const auto torrentPath = writeTorrentFile();
	const auto downloadPath = testDirectory / "downloads";
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));
	const auto hash = manager.getTorrentSnapshot().front().hash;

	manager.requestStatusRefresh();
	std::optional<lt::torrent_status> status;
	for (int attempt = 0; attempt < 100 && !status; ++attempt)
	{
		status = manager.getCachedStatus(hash);
		if (!status)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	ASSERT_TRUE(status);
	EXPECT_EQ(status->name, "fixture");
}

TEST_F(TorrentManagerTest, PublishesAStableStatusRevisionBetweenRefreshes)
{
	TorrentManager manager;
	const auto torrentPath = writeTorrentFile();
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), (testDirectory / "downloads").string()));

	const auto before = manager.getStatusRevision();
	manager.requestStatusRefresh();
	std::uint64_t refreshed = before;
	for (int attempt = 0; attempt < 100 && refreshed == before; ++attempt)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		refreshed = manager.getStatusRevision();
	}
	ASSERT_GT(refreshed, before);

	manager.requestStatusRefresh();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(manager.getStatusRevision(), refreshed);
}

TEST_F(TorrentManagerTest, EventDrainingDoesNotInterfereWithPersistenceAlerts)
{
	TorrentManager manager;
	const auto torrentPath = writeTorrentFile();
	const auto downloadPath = testDirectory / "downloads";
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), downloadPath.string()));

	std::atomic<bool> stopDrain{false};
	std::thread drainWorker([&manager, &stopDrain] {
		while (!stopDrain.load())
		{
			auto events = manager.drainEvents();
			(void)events;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	std::vector<ManagedTorrent> snapshot;
	Result persistenceResult = manager.getPersistenceSnapshot(snapshot, std::chrono::seconds(2));
	stopDrain.store(true);
	if (drainWorker.joinable())
		drainWorker.join();

	ASSERT_TRUE(persistenceResult);
	ASSERT_EQ(snapshot.size(), 1u);
	EXPECT_FALSE(snapshot.front().resumeData.empty());
}

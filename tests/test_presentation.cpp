#include <gtest/gtest.h>

#include "presentation/SearchPresenter.hpp"
#include "presentation/TorrentListPresenter.hpp"
#include "presentation/UiFormatters.hpp"
#include "presentation/TorrentAvailability.hpp"
#include "utils/TorrentIdentity.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>
#include <cctype>

namespace
{
template <typename Digest>
Digest digestWithBinaryBytes()
{
	Digest digest;
	for (std::size_t index = 0; index < digest.size(); ++index)
		digest.data()[index] = static_cast<char>((index * 37 + 0xff) & 0xff);
	digest.data()[0] = 0;
	digest.data()[1] = static_cast<char>(0xff);
	digest.data()[2] = static_cast<char>(0xc3);
	digest.data()[3] = static_cast<char>(0x28);
	return digest;
}

TEST(TorrentIdentityTest, EncodesBinaryV1V2AndHybridHashesAsCanonicalAscii)
{
	const auto v1 = digestWithBinaryBytes<lt::sha1_hash>();
	const auto v2 = digestWithBinaryBytes<lt::sha256_hash>();
	const std::string v1Id = Utils::TorrentIdentity::id(lt::info_hash_t(v1));
	const std::string v2Id = Utils::TorrentIdentity::id(lt::info_hash_t(v2));
	const std::string hybridId = Utils::TorrentIdentity::id(lt::info_hash_t(v1, v2));
	EXPECT_EQ(v1Id.size(), 43U);
	EXPECT_EQ(v2Id.size(), 67U);
	EXPECT_EQ(hybridId.size(), 111U);
	EXPECT_TRUE(v1Id.starts_with("v1:"));
	EXPECT_TRUE(v2Id.starts_with("v2:"));
	EXPECT_TRUE(hybridId.starts_with("v1:"));
	EXPECT_NE(hybridId.find("|v2:"), std::string::npos);
	EXPECT_TRUE(Utils::TorrentIdentity::isValid(v1Id));
	EXPECT_TRUE(Utils::TorrentIdentity::isValid(v2Id));
	EXPECT_TRUE(Utils::TorrentIdentity::isValid(hybridId));
	EXPECT_TRUE(std::all_of(hybridId.begin(), hybridId.end(), [](unsigned char character)
	{
		return character >= 0x20 && character <= 0x7e;
	}));
}

TEST(TorrentAvailabilityTest, MapsEveryStateToOneCentralMessage)
{
	using enum Presentation::TorrentAvailability;
	EXPECT_EQ(Presentation::availabilityMessage({LoadingStatus, {}}), "Loading torrent status...");
	EXPECT_EQ(Presentation::availabilityMessage({MetadataPending, {}}), "Waiting for torrent metadata...");
	EXPECT_EQ(Presentation::availabilityMessage({Removed, {}}), "The selected torrent was removed.");
	EXPECT_EQ(Presentation::availabilityMessage({InvalidId, {}}), "Internal torrent identifier error.");
	EXPECT_EQ(Presentation::availabilityMessage({Error, "Tracker failed"}), "Tracker failed");
}

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

TEST(TorrentListPresenterTest, KeepsSelectionIdentityWhileStatusesAreLoading)
{
	const auto testDirectory = std::filesystem::temp_directory_path()
		/ ("hypertube-presenter-loading-" + std::to_string(
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
	const auto hash = manager.getTorrentSnapshot().front().hash;
	const std::string id = Presentation::TorrentListPresenter::idForHash(hash);
	ASSERT_EQ(id.size(), 43U);
	EXPECT_TRUE(Utils::TorrentIdentity::isValid(id));

	Presentation::TorrentListPresenter presenter(manager);
	presenter.setSelectedId(id);
	EXPECT_TRUE(presenter.buildRows().empty());
	EXPECT_EQ(presenter.selectedId(), id);
	EXPECT_TRUE(presenter.hashForId(id).has_value());
	const auto selectedRow = presenter.findRowById(id);
	ASSERT_TRUE(selectedRow);
	EXPECT_EQ(selectedRow->id, id);
	EXPECT_FALSE(selectedRow->name.empty());

	std::error_code error;
	std::filesystem::remove_all(testDirectory, error);
}

TEST(TorrentListPresenterTest, SelectionSurvivesRefreshAndSortUntilTorrentIsRemoved)
{
	const auto testDirectory = std::filesystem::temp_directory_path()
		/ ("hypertube-presenter-lifecycle-" + std::to_string(
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
	Presentation::TorrentListPresenter presenter(manager);
	const auto hash = manager.getTorrentSnapshot().front().hash;
	const std::string id = Presentation::TorrentListPresenter::idForHash(hash);
	presenter.setSelectedId(id);
	manager.requestStatusRefresh();
	EXPECT_TRUE(presenter.hashForId(id));
	EXPECT_EQ(presenter.selectedId(), id);
	presenter.setSort(Presentation::TorrentSortField::Name, false);
	(void)presenter.buildRows();
	EXPECT_TRUE(presenter.hashForId(id));
	manager.requestStatusRefresh();
	EXPECT_TRUE(presenter.hashForId(id));
	EXPECT_NE(presenter.availabilityForId(id).state, Presentation::TorrentAvailability::Removed);
	ASSERT_TRUE(manager.removeTorrent(hash, TorrentRemovalMode::KeepAllFiles));
	EXPECT_EQ(presenter.availabilityForId(id).state, Presentation::TorrentAvailability::Removed);
	EXPECT_EQ(Presentation::availabilityMessage(presenter.availabilityForId(id)),
		"The selected torrent was removed.");

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

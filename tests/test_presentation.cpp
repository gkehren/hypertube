#include <gtest/gtest.h>

#include "presentation/SearchPresenter.hpp"
#include "presentation/TorrentListPresenter.hpp"
#include "presentation/UiFormatters.hpp"
#include "presentation/UiNotifications.hpp"
#include "presentation/TorrentAvailability.hpp"
#include "utils/TorrentIdentity.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>
#include <cctype>
#include <vector>

namespace
{
std::filesystem::path writeNamedTorrent(const std::filesystem::path &directory, const std::string &name)
{
	const auto path = directory / (name + ".torrent");
	std::string content = "d4:infod6:lengthi1e4:name" + std::to_string(name.size()) + ":" + name
		+ "12:piece lengthi16384e6:pieces20:";
	content.append(20, '\0');
	content += "ee";
	std::ofstream file(path, std::ios::binary);
	file.write(content.data(), static_cast<std::streamsize>(content.size()));
	return path;
}

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

TEST(UiFormattersTest, ParsesAndFormatsBinarySpeedLimits)
{
	int bytesPerSecond = 0;
	EXPECT_TRUE(Presentation::UiFormatters::parseSpeedLimit("2048", bytesPerSecond));
	EXPECT_EQ(bytesPerSecond, 2048);
	EXPECT_TRUE(Presentation::UiFormatters::parseSpeedLimit("1.5 MiB/s", bytesPerSecond));
	EXPECT_EQ(bytesPerSecond, 1572864);
	EXPECT_TRUE(Presentation::UiFormatters::parseSpeedLimit("1.5 GiB", bytesPerSecond));
	EXPECT_EQ(bytesPerSecond, 1610612736);
	EXPECT_EQ(Presentation::UiFormatters::formatSpeedLimit(0), "0");
	EXPECT_EQ(Presentation::UiFormatters::formatSpeedLimit(1024), "1 KiB/s");
	EXPECT_EQ(Presentation::UiFormatters::formatSpeedLimit(1572864), "1.5 MiB/s");
	EXPECT_EQ(Presentation::UiFormatters::formatSpeedLimit(std::numeric_limits<int>::max()), "2147483647 B/s");
}

TEST(UiFormattersTest, RejectsMalformedAndOverflowingSpeedLimits)
{
	int bytesPerSecond = 0;
	EXPECT_FALSE(Presentation::UiFormatters::parseSpeedLimit("", bytesPerSecond));
	EXPECT_FALSE(Presentation::UiFormatters::parseSpeedLimit("-1 MiB/s", bytesPerSecond));
	EXPECT_FALSE(Presentation::UiFormatters::parseSpeedLimit("12 Mbps", bytesPerSecond));
	EXPECT_FALSE(Presentation::UiFormatters::parseSpeedLimit("2 GiB/s", bytesPerSecond));
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

TEST(UiNotificationTest, NotificationQueueEnqueuesAndExpires)
{
	Presentation::NotificationQueue queue(4);
	Presentation::UiNotification notif{Presentation::NotificationSeverity::Info, "Title", "Message", {}, {}, std::chrono::milliseconds(50)};
	const auto now = std::chrono::steady_clock::now();
	queue.enqueue(notif);
	ASSERT_TRUE(queue.current());
	EXPECT_EQ(queue.current()->message, "Message");

	queue.tick(now + std::chrono::milliseconds(10));
	EXPECT_TRUE(queue.current());

	queue.tick(now + std::chrono::milliseconds(100));
	EXPECT_FALSE(queue.current());
}

TEST(UiNotificationTest, NotificationQueueCoalescesEquivalentMessages)
{
	Presentation::NotificationQueue queue(4);
	Presentation::UiNotification notif{Presentation::NotificationSeverity::Info, "Title", "Dup", {}, {}, std::chrono::milliseconds(100)};
	queue.enqueue(notif);
	queue.enqueue(notif);
	EXPECT_EQ(queue.pendingCount(), 0U);
	ASSERT_TRUE(queue.current());
	EXPECT_EQ(queue.current()->message, "Dup");
}

TEST(UiNotificationTest, NotificationQueueIsBounded)
{
	Presentation::NotificationQueue queue(2);
	Presentation::UiNotification n1{Presentation::NotificationSeverity::Info, "T", "1", {}, {}, std::chrono::milliseconds(100)};
	Presentation::UiNotification n2{Presentation::NotificationSeverity::Info, "T", "2", {}, {}, std::chrono::milliseconds(100)};
	Presentation::UiNotification n3{Presentation::NotificationSeverity::Info, "T", "3", {}, {}, std::chrono::milliseconds(100)};
	Presentation::UiNotification n4{Presentation::NotificationSeverity::Info, "T", "4", {}, {}, std::chrono::milliseconds(100)};

	queue.enqueue(n1); // active
	queue.enqueue(n2); // pending 1
	queue.enqueue(n3); // pending 2
	queue.enqueue(n4); // drops n2, pending 2 (n3, n4)
	EXPECT_EQ(queue.pendingCount(), 2U);
}

TEST(UiNotificationTest, NotificationQueueDismissesCurrent)
{
	Presentation::NotificationQueue queue(4);
	Presentation::UiNotification n1{Presentation::NotificationSeverity::Info, "T", "1", {}, {}, std::chrono::milliseconds(1000)};
	Presentation::UiNotification n2{Presentation::NotificationSeverity::Info, "T", "2", {}, {}, std::chrono::milliseconds(1000)};
	queue.enqueue(n1);
	queue.enqueue(n2);
	EXPECT_EQ(queue.current()->message, "1");
	queue.dismiss();
	ASSERT_TRUE(queue.current());
	EXPECT_EQ(queue.current()->message, "2");
}

TEST(UiNotificationTest, ErrorNotificationUsesMinimumDisplayDuration)
{
	Presentation::NotificationQueue queue(4);
	Presentation::UiNotification err{Presentation::NotificationSeverity::Error, "Err", "Failed", {}, {}, std::chrono::milliseconds(100)};
	const auto now = std::chrono::steady_clock::now();
	queue.enqueue(err);
	ASSERT_TRUE(queue.current());
	// Must stay active at 5 seconds because minimum error duration is 8s
	queue.tick(now + std::chrono::seconds(5));
	EXPECT_TRUE(queue.current());
	// Expires at 9 seconds (> 8s)
	queue.tick(now + std::chrono::seconds(9));
	EXPECT_FALSE(queue.current());
}

TEST(UiNotificationTest, NotificationQueueAdvancesAfterExpiration)
{
	Presentation::NotificationQueue queue(4);
	Presentation::UiNotification n1{Presentation::NotificationSeverity::Success, "T", "1", {}, {}, std::chrono::milliseconds(50)};
	Presentation::UiNotification n2{Presentation::NotificationSeverity::Warning, "T", "2", {}, {}, std::chrono::milliseconds(50)};
	const auto now = std::chrono::steady_clock::now();
	queue.enqueue(n1);
	queue.enqueue(n2);
	EXPECT_EQ(queue.current()->message, "1");
	queue.tick(now + std::chrono::milliseconds(100));
	ASSERT_TRUE(queue.current());
	EXPECT_EQ(queue.current()->message, "2");
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
	const auto rows = presenter.buildRows();
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_EQ(rows.front().stateLabel, "Loading");
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

TEST(TorrentListPresenterTest, SupportsRangeToggleSelectAllAndPartialBatchResults)
{
	const auto testDirectory = std::filesystem::temp_directory_path()
		/ ("hypertube-presenter-multi-selection-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(testDirectory / "downloads");
	TorrentManager manager;
	for (const auto &name : {std::string("alpha"), std::string("bravo"), std::string("charlie")})
		ASSERT_TRUE(manager.addTorrent(writeNamedTorrent(testDirectory, name).string(),
			(testDirectory / "downloads").string()));

	manager.requestStatusRefresh();
	for (int attempt = 0; attempt < 100 && (!manager.getStatusCache() || manager.getStatusCache()->size() < 3); ++attempt)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	Presentation::TorrentListPresenter presenter(manager);
	presenter.setSort(Presentation::TorrentSortField::Name, true);
	const auto rows = presenter.buildRows();
	ASSERT_EQ(rows.size(), 3U);

	presenter.selectVisibleId(rows[0].id, false, false);
	presenter.selectVisibleId(rows[2].id, false, true);
	EXPECT_EQ(presenter.selectedCount(), 3U);
	EXPECT_EQ(presenter.selectedId(), rows[2].id);

	presenter.selectVisibleId(rows[1].id, true, false);
	EXPECT_EQ(presenter.selectedCount(), 2U);
	EXPECT_FALSE(presenter.isSelected(rows[1].id));
	presenter.selectAllVisible();
	EXPECT_EQ(presenter.selectedCount(), 3U);

	const auto partial = presenter.executeCommand({rows[0].id, "not-a-torrent-id"}, TorrentCommand::Pause);
	EXPECT_EQ(partial.requested, 2U);
	EXPECT_EQ(partial.succeeded, 1U);
	ASSERT_EQ(partial.failures.size(), 1U);
	EXPECT_EQ(partial.failures.front().id, "not-a-torrent-id");

	ASSERT_TRUE(manager.removeTorrent(manager.getTorrentSnapshot().front().hash, TorrentRemovalMode::KeepAllFiles));
	presenter.buildRows();
	EXPECT_EQ(presenter.selectedCount(), 2U);
	EXPECT_EQ(presenter.selectedIds().size(), 2U);

	std::error_code error;
	std::filesystem::remove_all(testDirectory, error);
}

TEST(TorrentListPresenterTest, SnapshotInvalidationAndIndexedLookupBehavior)
{
	const auto testDirectory = std::filesystem::temp_directory_path()
		/ ("hypertube-presenter-snapshot-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(testDirectory / "downloads");
	TorrentManager manager;
	ASSERT_TRUE(manager.addTorrent(writeNamedTorrent(testDirectory, "test1").string(), (testDirectory / "downloads").string()));
	manager.refreshStatusCache();

	Presentation::TorrentListPresenter presenter(manager);

	// Test A: Initial build creates snapshot and indexed lookup work O(1)
	const auto rows1 = presenter.buildRows();
	ASSERT_EQ(rows1.size(), 1U);
	const std::string id = rows1.front().id;

	const auto found1 = presenter.findRowById(id);
	ASSERT_TRUE(found1.has_value());
	EXPECT_EQ(found1->id, id);

	// Test B: Filter/sort change reuses base snapshot (does not re-query libtorrent collection/status)
	presenter.setTextFilter("test1");
	const auto rows2 = presenter.buildRows();
	ASSERT_EQ(rows2.size(), 1U);

	presenter.setSort(Presentation::TorrentSortField::Name, false);
	const auto rows3 = presenter.buildRows();
	ASSERT_EQ(rows3.size(), 1U);

	// Test C: Status revision change forces snapshot update
	manager.refreshStatusCache();
	const auto rows4 = presenter.buildRows();
	ASSERT_EQ(rows4.size(), 1U);

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

namespace
{
double srgbToLinear(double channel)
{
	channel /= 255.0;
	return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(std::uint32_t hexColor)
{
	const double r = srgbToLinear((hexColor >> 16) & 0xff);
	const double g = srgbToLinear((hexColor >> 8) & 0xff);
	const double b = srgbToLinear(hexColor & 0xff);
	return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double contrastRatio(std::uint32_t c1, std::uint32_t c2)
{
	const double l1 = relativeLuminance(c1);
	const double l2 = relativeLuminance(c2);
	const double lighter = std::max(l1, l2);
	const double darker = std::min(l1, l2);
	return (lighter + 0.05) / (darker + 0.05);
}
} // namespace

TEST(ThemeTokensTest, ContrastRatioRequirementsForSupportedThemes)
{
	struct ThemePair {
		const char *name;
		std::uint32_t background;
		std::uint32_t foreground;
		double minRatio;
	};

	const ThemePair themes[] = {
		{ "dark", 0x15181d, 0xf5f7fa, 4.5 },
		{ "light", 0xf5f7fa, 0x18202a, 4.5 },
		{ "high-contrast", 0x000000, 0xffffff, 7.0 },
		{ "ocean", 0x101c2c, 0xe6f4ff, 4.5 },
		{ "nord", 0x2e3440, 0xeceff4, 4.5 },
		{ "dracula", 0x282a36, 0xf8f8f2, 4.5 },
		{ "cyberpunk", 0x100d1a, 0xf5f0ff, 4.5 }
	};

	for (const auto &pair : themes)
	{
		const double ratio = contrastRatio(pair.background, pair.foreground);
		EXPECT_GE(ratio, pair.minRatio) << "Theme " << pair.name << " contrast ratio (" << ratio << ") below minimum " << pair.minRatio;
	}
}

TEST(ThemeTokensTest, SelectionColorsRemainReadableAcrossSupportedThemes)
{
	struct SelectionPair {
		const char *name;
		std::uint32_t selection;
		std::uint32_t foreground;
	};
	const SelectionPair selections[] = {
		{ "dark", 0x293448, 0xf5f7fa },
		{ "light", 0xcfe0f7, 0x18202a },
		{ "high-contrast", 0x004d40, 0xffffff },
		{ "ocean", 0x244967, 0xe6f4ff },
		{ "nord", 0x4c566a, 0xeceff4 },
		{ "dracula", 0x535d8a, 0xf8f8f2 },
		{ "cyberpunk", 0x482b72, 0xf5f0ff }
	};

	for (const auto &pair : selections)
		EXPECT_GE(contrastRatio(pair.selection, pair.foreground), 4.5)
			<< "Theme " << pair.name << " selection contrast is too low";
}
} // namespace

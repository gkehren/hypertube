#include <gtest/gtest.h>
#include "App.hpp"
#include "AppPaths.hpp"
#include "presentation/PreferencesController.hpp"
#include "presentation/UiStateController.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

struct AppShutdownTestFixture {
	AppShutdownTestFixture() {
		Utils::AppPaths::resetPortableCache();
		testDir = std::filesystem::temp_directory_path() / ("hypertube_app_shutdown_test_" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(testDir);
		const auto marker = testDir / "portable.mode";
		std::ofstream(marker) << "";
		Utils::AppPaths::setOverrideExecutableDirectory(testDir);

		EXPECT_TRUE(Utils::AppPaths::isPortable());
		EXPECT_EQ(Utils::AppPaths::configDirectory(), testDir / "config");
		EXPECT_EQ(Utils::AppPaths::dataDirectory(), testDir / "data");
	}
	~AppShutdownTestFixture() {
		Utils::AppPaths::resetPortableCache();
		std::error_code ec;
		std::filesystem::remove_all(testDir, ec);
	}
	std::filesystem::path testDir;
};

TEST(AppShutdownTest, CleanShutdownIsIdempotent)
{
	AppShutdownTestFixture fixture;
	App app;
	Result res = app.initialize();
	ASSERT_TRUE(res);

	// First shutdown call - performs persistence
	app.shutdown();
	const auto torrentsJson = fixture.testDir / "config" / "torrents.json";
	ASSERT_TRUE(std::filesystem::exists(torrentsJson));
	const auto firstWriteTime = std::filesystem::last_write_time(torrentsJson);

	// Second and third shutdown calls must be harmless and idempotent
	app.shutdown();
	app.shutdown();
	const auto thirdWriteTime = std::filesystem::last_write_time(torrentsJson);
	EXPECT_EQ(firstWriteTime, thirdWriteTime);
}

TEST(AppShutdownTest, ShutdownWithPendingPreferencesAndUiStateSave)
{
	AppShutdownTestFixture fixture;
	App app;
	ASSERT_TRUE(app.initialize());

	Presentation::PreferencesController preferencesController(
		app.torrentManager(), app.searchEngine(), app.settingsConfigManager(),
		[](int) {}, {}, Utils::AppPaths::settingsConfigPath().string());

	Presentation::UiStateSnapshot initialSnapshot{0, {250, 200, false, 0, 0}};
	Presentation::UiStateController uiStateController(
		preferencesController, initialSnapshot,
		[](const Presentation::UiStateSnapshot &) {},
		[](const Result &) {});

	// Modify UI state snapshot and trigger pending save
	Presentation::UiStateSnapshot modifiedSnapshot = initialSnapshot;
	modifiedSnapshot.layout.sidebarWidth = 340;
	modifiedSnapshot.layout.bottomPanelHeight = 220;
	uiStateController.request(modifiedSnapshot);

	EXPECT_TRUE(uiStateController.hasPending());

	// Exercise shutdown flow sequence:
	// Flush pending UI state -> Wait for preferences save -> Shutdown App
	Result flushResult = uiStateController.flush();
	EXPECT_TRUE(flushResult);

	Result prefResult = preferencesController.waitForSave();
	EXPECT_TRUE(prefResult);

	app.shutdown();

	// Verify updated UI layout settings persisted cleanly to settingsConfigManager
	const auto savedPreferences = app.settingsConfigManager().getPreferencesSettings();
	EXPECT_EQ(savedPreferences.ui.sidebarWidth, 340);
	EXPECT_EQ(savedPreferences.ui.bottomPanelHeight, 220);
}

} // namespace

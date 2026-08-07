#include <gtest/gtest.h>

#include "ConfigManager.hpp"
#include "SearchEngine.hpp"
#include "TorrentManager.hpp"
#include "presentation/PreferencesController.hpp"
#include "presentation/UiStateController.hpp"

#include <chrono>
#include <filesystem>
#include <map>
#include <thread>

namespace
{
struct TempDirectory
{
	TempDirectory()
	{
		path = std::filesystem::temp_directory_path() /
			("hypertube_preferences_test_" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(path);
	}
	~TempDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
	std::filesystem::path path;
};

Presentation::PreferencesController::CredentialStoreOps fakeStore(std::map<std::string, std::string> &values)
{
	return {
		[&values](const std::string &account, const std::string &secret) {
			values[account] = secret;
			return Result::Success();
		},
		[&values](const std::string &account) {
			values.erase(account);
			return Result::Success();
		},
		[&values](const std::string &account) -> std::optional<std::string> {
			const auto found = values.find(account);
			return found == values.end() ? std::nullopt : std::optional<std::string>(found->second);
		}};
}

TEST(PreferencesControllerTest, RestoresProxySecretAfterEraseAndFailedSave)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets{{"proxy_password", "old-password"}};
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	PreferencesSettings settings = configManager.getPreferencesSettings();
	settings.proxyEnabled = false;

	// An existing directory is not a valid JSON target. The asynchronous save
	// therefore fails after the credential erase has already succeeded.
	const auto failedTarget = temp.path / "settings-target";
	std::filesystem::create_directories(failedTarget);
	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), failedTarget.string());

	ASSERT_TRUE(controller.beginSave(settings));
	const Result result = controller.waitForSave();
	EXPECT_FALSE(result);
	ASSERT_TRUE(secrets.contains("proxy_password"));
	EXPECT_EQ(secrets.at("proxy_password"), "old-password");
}

TEST(PreferencesControllerTest, UiStateSaveSkipsInvalidNetworkValidation)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets;
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	PreferencesSettings settings = configManager.getPreferencesSettings();
	settings.proxyEnabled = true;
	settings.proxyHost.clear();
	settings.proxyPort = 0;

	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), (temp.path / "settings.json").string());
	ASSERT_TRUE(controller.beginUiStateSave(settings));
	ASSERT_TRUE(controller.waitForSave());
	EXPECT_TRUE(configManager.getPreferencesSettings().proxyEnabled);
	EXPECT_EQ(configManager.getPreferencesSettings().ui.sidebarWidth, settings.ui.sidebarWidth);
}

TEST(PreferencesControllerTest, PreservesReplacesAndExplicitlyClearsSecrets)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets{{"proxy_password", "old-password"}, {"torznab_api_key", "old-key"}};
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	PreferencesSettings settings = configManager.getPreferencesSettings();
	settings.proxyEnabled = false;
	settings.torznabEnabled = false;
	const auto path = (temp.path / "settings.json").string();

	// Disabling proxy or torznab with std::nullopt secrets preserves existing stored credentials
	Presentation::PreferencesController preserve(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), path);
	ASSERT_TRUE(preserve.beginSave(settings));
	ASSERT_TRUE(preserve.waitForSave());
	EXPECT_EQ(secrets.at("proxy_password"), "old-password");
	EXPECT_EQ(secrets.at("torznab_api_key"), "old-key");

	// Replacing proxy secret with non-empty optional
	Presentation::PreferencesController replace(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), path);
	ASSERT_TRUE(replace.beginSave(settings, std::optional<std::string>("new-key"), std::optional<std::string>("new-password")));
	ASSERT_TRUE(replace.waitForSave());
	EXPECT_EQ(secrets.at("proxy_password"), "new-password");
	EXPECT_EQ(secrets.at("torznab_api_key"), "new-key");

	// Explicitly passing empty string erases the secret
	Presentation::PreferencesController clear(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), path);
	ASSERT_TRUE(clear.beginSave(settings, std::optional<std::string>(""), std::optional<std::string>("")));
	ASSERT_TRUE(clear.waitForSave());
	EXPECT_FALSE(secrets.contains("proxy_password"));
	EXPECT_FALSE(secrets.contains("torznab_api_key"));
}

TEST(PreferencesControllerTest, NoSecretMutationDoesNotTouchCredentialStore)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets{{"proxy_password", "my-secret"}};
	std::size_t storeCalls = 0;
	std::size_t eraseCalls = 0;
	Presentation::PreferencesController::CredentialStoreOps trackingOps{
		[&secrets, &storeCalls](const std::string &account, const std::string &secret) {
			storeCalls++;
			secrets[account] = secret;
			return Result::Success();
		},
		[&secrets, &eraseCalls](const std::string &account) {
			eraseCalls++;
			secrets.erase(account);
			return Result::Success();
		},
		[&secrets](const std::string &account) -> std::optional<std::string> {
			const auto found = secrets.find(account);
			return found == secrets.end() ? std::nullopt : std::optional<std::string>(found->second);
		}
	};
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	PreferencesSettings settings = configManager.getPreferencesSettings();
	const auto path = (temp.path / "settings.json").string();

	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, trackingOps, path);
	ASSERT_TRUE(controller.beginSave(settings));
	ASSERT_TRUE(controller.waitForSave());

	EXPECT_EQ(storeCalls, 0u);
	EXPECT_EQ(eraseCalls, 0u);
	EXPECT_EQ(secrets.at("proxy_password"), "my-secret");
}

TEST(PreferencesControllerTest, QueuedUiStateDoesNotOverwriteNetworkTransaction)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets;
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	const auto path = (temp.path / "settings.json").string();
	PreferencesSettings network = configManager.getPreferencesSettings();
	network.downloadSpeedLimit = 12345;
	PreferencesSettings ui = network;
	ui.theme = 4;
	ui.ui.sidebarWidth = 500;

	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), path);
	ASSERT_TRUE(controller.beginSave(network));
	ASSERT_TRUE(controller.beginUiStateSave(ui));
	ASSERT_TRUE(controller.waitForSave());
	ASSERT_TRUE(controller.waitForSave());

	const auto committed = configManager.getPreferencesSettings();
	EXPECT_EQ(committed.downloadSpeedLimit, 12345);
	EXPECT_EQ(committed.theme, 4);
	EXPECT_EQ(committed.ui.sidebarWidth, 500);
}

TEST(UiStateControllerTest, CoalescesChangesAndCommitsLatestSnapshot)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets;
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	Presentation::PreferencesController preferences(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), (temp.path / "settings.json").string());
	const auto initial = preferences.current();
	Presentation::UiStateController controller(preferences, {initial.theme, initial.ui});

	auto first = controller.committed();
	first.layout.sidebarWidth = 300;
	controller.request(first);
	auto latest = first;
	latest.layout.sidebarWidth = 420;
	controller.request(latest);
	std::this_thread::sleep_for(std::chrono::milliseconds(450));
	controller.poll();
	ASSERT_TRUE(controller.flush());

	EXPECT_EQ(preferences.current().ui.sidebarWidth, 420);
	EXPECT_EQ(controller.committed().layout.sidebarWidth, 420);
}

TEST(UiStateControllerTest, PersistsChangeRequestedWhilePreviousSnapshotIsInFlight)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets;
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	Presentation::PreferencesController preferences(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), (temp.path / "settings.json").string());
	const auto initial = preferences.current();
	Presentation::UiStateController controller(preferences, {initial.theme, initial.ui});

	auto first = controller.committed();
	first.layout.sidebarWidth = 300;
	controller.request(first);
	std::this_thread::sleep_for(std::chrono::milliseconds(450));
	controller.poll();

	auto latest = first;
	latest.layout.sidebarWidth = 420;
	controller.request(latest);
	ASSERT_TRUE(controller.flush());

	EXPECT_EQ(preferences.current().ui.sidebarWidth, 420);
	EXPECT_EQ(configManager.getPreferencesSettings().ui.sidebarWidth, 420);
	EXPECT_EQ(controller.committed().layout.sidebarWidth, 420);
}
} // namespace

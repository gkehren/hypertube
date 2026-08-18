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
#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#endif

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
		[&values](const std::string &account) -> Utils::CredentialStore::CredentialLoadResult {
			const auto found = values.find(account);
			return found == values.end()
				? Utils::CredentialStore::CredentialLoadResult{Utils::CredentialStore::CredentialStatus::Missing, ""}
				: Utils::CredentialStore::CredentialLoadResult{Utils::CredentialStore::CredentialStatus::Stored, found->second};
		}};
}

#ifndef _WIN32
class SlowHttpServer
{
public:
	SlowHttpServer()
	{
		listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
		if (listenFd_ < 0)
		{
			return;
		}
		int reuse = 1;
		setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(0);
		if (bind(listenFd_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0
			|| listen(listenFd_, 1) != 0)
		{
			close(listenFd_);
			listenFd_ = -1;
			return;
		}
		socklen_t length = sizeof(address);
		if (getsockname(listenFd_, reinterpret_cast<sockaddr *>(&address), &length) != 0)
		{
			close(listenFd_);
			listenFd_ = -1;
			return;
		}
		url_ = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/api";
		worker_ = std::thread([this] {
			const int client = accept(listenFd_, nullptr, nullptr);
			clientFd_.store(client);
			while (!stopping_.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			if (client >= 0)
				close(client);
		});
	}

	~SlowHttpServer()
	{
		stopping_ = true;
		if (listenFd_ >= 0)
		{
			shutdown(listenFd_, SHUT_RDWR);
			close(listenFd_);
		}
		const int client = clientFd_.load();
		if (client >= 0)
			shutdown(client, SHUT_RDWR);
		if (worker_.joinable())
			worker_.join();
	}

	bool valid() const { return listenFd_ >= 0; }
	const std::string &url() const { return url_; }

private:
	int listenFd_ = -1;
	std::atomic<int> clientFd_{-1};
	std::atomic<bool> stopping_{false};
	std::thread worker_;
	std::string url_;
};
#endif

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

TEST(PreferencesControllerTest, ConnectionTestRejectsInvalidCandidateWithoutSaving)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets;
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	const PreferencesSettings before = configManager.getPreferencesSettings();
	PreferencesSettings candidate = before;
	candidate.torznabEnabled = true;
	candidate.torznabUrl = "localhost:9117/api";

	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), (temp.path / "settings.json").string());
	const Result result = controller.beginConnectionTest(candidate);
	EXPECT_FALSE(result);
	EXPECT_EQ(result.code, ResultCode::InvalidInput);
	EXPECT_FALSE(controller.isConnectionTestRunning());
	EXPECT_EQ(configManager.getPreferencesSettings().torznabEnabled, before.torznabEnabled);
	EXPECT_EQ(configManager.getPreferencesSettings().torznabUrl, before.torznabUrl);
}

TEST(PreferencesControllerTest, ConnectionCancellationIsBoundedAndCanBeFollowedByANewTest)
{
#ifdef _WIN32
	GTEST_SKIP() << "The local slow HTTP fixture is implemented for POSIX test hosts";
#else
	SlowHttpServer server;
	if (!server.valid())
		GTEST_SKIP() << "Local socket creation is unavailable in this environment";
	std::map<std::string, std::string> secrets{{"torznab_api_key", "stored-key"}};
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	PreferencesSettings candidate = configManager.getPreferencesSettings();
	candidate.torznabEnabled = true;
	candidate.torznabUrl = server.url();
	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), (std::filesystem::temp_directory_path() / "hypertube-cancel-test.json").string());

	ASSERT_TRUE(controller.beginConnectionTest(candidate));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	const auto cancelStart = std::chrono::steady_clock::now();
	const Result cancellation = controller.cancelConnectionTest();
	const auto cancelElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - cancelStart);
	EXPECT_TRUE(cancellation);
	EXPECT_LT(cancelElapsed.count(), 250);

	std::optional<Result> completed;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (!completed && std::chrono::steady_clock::now() < deadline)
	{
		completed = controller.pollConnectionTest();
		if (!completed)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ASSERT_TRUE(completed.has_value());
	EXPECT_EQ(completed->code, ResultCode::Cancelled);
	EXPECT_EQ(secrets.at("torznab_api_key"), "stored-key");

	PreferencesSettings second = candidate;
	second.torznabUrl = "http://127.0.0.1:1/api";
	ASSERT_TRUE(controller.beginConnectionTest(second));
	const Result secondResult = controller.waitForConnectionTest();
	EXPECT_NE(secondResult.code, ResultCode::Busy);
#endif
}

TEST(PreferencesControllerTest, ProxyTestDoesNotRequireTorznabToBeEnabled)
{
	TempDirectory temp;
	std::map<std::string, std::string> secrets{{"proxy_password", "stored-proxy-password"}};
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	PreferencesSettings candidate = configManager.getPreferencesSettings();
	candidate.torznabEnabled = false;
	candidate.proxyEnabled = true;
	candidate.proxyType = "http";
	candidate.proxyHost = "127.0.0.1";
	candidate.proxyPort = 1;
	Presentation::PreferencesController controller(torrentManager, searchEngine, configManager,
		{}, fakeStore(secrets), (temp.path / "settings.json").string());

	ASSERT_TRUE(controller.beginProxyConnectionTest(candidate));
	const Result result = controller.waitForConnectionTest();
	EXPECT_EQ(result.code, ResultCode::Network);
	EXPECT_NE(result.message.find("Proxy connection"), std::string::npos);
	EXPECT_EQ(secrets.at("proxy_password"), "stored-proxy-password");
	EXPECT_FALSE(configManager.getPreferencesSettings().torznabEnabled);
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
		[&secrets](const std::string &account) -> Utils::CredentialStore::CredentialLoadResult {
			const auto found = secrets.find(account);
			return found == secrets.end()
				? Utils::CredentialStore::CredentialLoadResult{Utils::CredentialStore::CredentialStatus::Missing, ""}
				: Utils::CredentialStore::CredentialLoadResult{Utils::CredentialStore::CredentialStatus::Stored, found->second};
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

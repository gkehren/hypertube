#include <gtest/gtest.h>

#include "ConfigManager.hpp"
#include "DetailsModelAdapter.hpp"
#include "SearchEngine.hpp"
#include "SlintRefreshCoordinators.hpp"
#include "SlintControllerFacades.hpp"
#include "TorrentManager.hpp"
#include "presentation/PreferencesController.hpp"
#include "presentation/UiStateController.hpp"
#include "presentation/TorrentListPresenter.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
TEST(SlintControllerTest, RowCallbackSelectsTorrentAndPublishesDetails)
{
	const auto directory = std::filesystem::temp_directory_path() / ("hypertube-slint-controller-"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory / "downloads");
	const auto torrentPath = directory / "fixture.torrent";
	std::string content = "d4:infod6:lengthi1e4:name7:fixture12:piece lengthi16384e6:pieces20:";
	content.append(20, '\0');
	content += "ee";
	{
		std::ofstream file(torrentPath, std::ios::binary);
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
	}

	TorrentManager manager;
	manager.setCacheRefreshInterval(0);
	ASSERT_TRUE(manager.addTorrent(torrentPath.string(), (directory / "downloads").string()));
	const auto hash = manager.getTorrentSnapshot().front().hash;
	for (int attempt = 0; attempt < 100 && !manager.getCachedStatus(hash); ++attempt)
	{
		manager.requestStatusRefresh();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ASSERT_TRUE(manager.getCachedStatus(hash));

	Presentation::TorrentListPresenter torrentPresenter(manager);
	Utils::SystemUtils::SystemOpener opener;
	Presentation::TorrentDetailsPresenter detailsPresenter(manager, opener);
	auto visibleRows = torrentPresenter.buildRows();
	ASSERT_EQ(visibleRows.size(), 1U);
	const std::string expectedId = visibleRows.front().id;
	auto window = MainWindow::create();
	DetailsModelAdapter detailsModel;
	int selectedTab = 0;
	SlintUi::DetailsRefreshCoordinator coordinator(torrentPresenter, detailsPresenter, detailsModel,
		*window, selectedTab, visibleRows);
	window->on_select_torrent([&](const slint::SharedString &sharedId, bool, bool)
	{
		const std::string id(sharedId.begin(), sharedId.end());
		torrentPresenter.setSelectedId(id);
		coordinator.reset();
		coordinator.refresh(AppTab::Torrents);
	});

	window->invoke_select_torrent(slint::SharedString(expectedId), false, false);
	EXPECT_EQ(torrentPresenter.selectedId(), expectedId);
	const auto selectedId = window->get_selected_torrent_id();
	const auto selectedName = window->get_selected_torrent_name();
	const auto detailsMessage = window->get_details_message();
	EXPECT_EQ(std::string(selectedId.begin(), selectedId.end()), expectedId);
	EXPECT_EQ(std::string(selectedName.begin(), selectedName.end()), "fixture");
	EXPECT_EQ(std::string(detailsMessage.begin(), detailsMessage.end()), "General");

	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

TEST(SlintControllerTest, ProxyTestSkipsTorznabValidation)
{
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	ConfigManager configManager;
	Presentation::PreferencesController preferences(torrentManager, searchEngine, configManager);
	Presentation::UiStateController uiState(preferences, {});
	auto window = MainWindow::create();
	SlintUi::PreferencesUiController controller(preferences, uiState, *window, [] {
		return Presentation::UiStateSnapshot();
	});

	window->set_preference_download_limit(slint::SharedString("0"));
	window->set_preference_upload_limit(slint::SharedString("0"));
	window->set_preference_torznab_enabled(true);
	window->set_preference_torznab_url(slint::SharedString("not-a-url"));
	window->set_preference_proxy_enabled(true);
	window->set_preference_proxy_type(slint::SharedString("http"));
	window->set_preference_proxy_host(slint::SharedString("127.0.0.1"));
	window->set_preference_proxy_port(slint::SharedString("1"));

	controller.testProxyConnection();

	EXPECT_TRUE(window->get_preference_test_running());
	const auto stateMessage = window->get_preferences_state_message();
	EXPECT_EQ(std::string(stateMessage.begin(), stateMessage.end()), "Testing proxy connection...");

	controller.cancelConnectionTest();
	preferences.waitForConnectionTest();
}
} // namespace

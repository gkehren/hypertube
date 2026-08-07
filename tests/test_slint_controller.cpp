#include <gtest/gtest.h>

#include "DetailsModelAdapter.hpp"
#include "SlintRefreshCoordinators.hpp"
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
	window->on_select_torrent([&](const slint::SharedString &sharedId)
	{
		const std::string id(sharedId.begin(), sharedId.end());
		torrentPresenter.setSelectedId(id);
		coordinator.reset();
		coordinator.refresh(AppTab::Torrents);
	});

	window->invoke_select_torrent(slint::SharedString(expectedId));
	EXPECT_EQ(torrentPresenter.selectedId(), expectedId);
	EXPECT_EQ(std::string(window->get_selected_torrent_id().begin(), window->get_selected_torrent_id().end()), expectedId);
	EXPECT_EQ(std::string(window->get_selected_torrent_name().begin(), window->get_selected_torrent_name().end()), "fixture");
	EXPECT_EQ(std::string(window->get_details_message().begin(), window->get_details_message().end()), "General");

	std::error_code error;
	std::filesystem::remove_all(directory, error);
}
} // namespace

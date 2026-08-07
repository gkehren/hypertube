#pragma once

#include "DetailsModelAdapter.hpp"
#include "LogModelAdapter.hpp"
#include "SearchModelAdapter.hpp"
#include "SlintModelAdapter.hpp"
#include "app/App.hpp"
#include "main-window.h"
#include "presentation/LogsPresenter.hpp"
#include "presentation/SearchPresenter.hpp"
#include "presentation/TorrentDetailsPresenter.hpp"
#include "presentation/TorrentListPresenter.hpp"
#include "SystemUtils.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SlintUi
{
class TorrentRefreshCoordinator
{
public:
	TorrentRefreshCoordinator(TorrentManager &manager, Presentation::TorrentListPresenter &presenter,
		SlintModelAdapter &model, std::shared_ptr<slint::VectorModel<CategoryRow>> categories,
		MainWindow &window, bool &viewDirty, std::vector<Presentation::TorrentRowDto> &visibleRows);
	void refresh(AppTab activeTab);

private:
	TorrentManager &manager_;
	Presentation::TorrentListPresenter &presenter_;
	SlintModelAdapter &model_;
	std::shared_ptr<slint::VectorModel<CategoryRow>> categories_;
	MainWindow &window_;
	bool &viewDirty_;
	std::vector<Presentation::TorrentRowDto> &visibleRows_;
	std::uint64_t lastStatusRevision_ = 0;
};

class SearchRefreshCoordinator
{
public:
	SearchRefreshCoordinator(Presentation::SearchPresenter &presenter, SearchEngine &engine,
		SearchModelAdapter &results, SearchModelAdapter &favorites,
		std::shared_ptr<slint::VectorModel<slint::SharedString>> recent, MainWindow &window);
	void refreshIfNeeded(AppTab activeTab);
	void forceRefresh();

private:
	Presentation::SearchPresenter &presenter_;
	SearchEngine &engine_;
	SearchModelAdapter &results_;
	SearchModelAdapter &favorites_;
	std::shared_ptr<slint::VectorModel<slint::SharedString>> recent_;
	MainWindow &window_;
	std::uint64_t lastRevision_ = 0;
};

class LogRefreshCoordinator
{
public:
	LogRefreshCoordinator(Presentation::LogsPresenter &presenter, LogModelAdapter &model, MainWindow &window);
	void refresh(AppTab activeTab);

private:
	Presentation::LogsPresenter &presenter_;
	LogModelAdapter &model_;
	MainWindow &window_;
	std::uint64_t lastRevision_ = 0;
};

class DetailsRefreshCoordinator
{
public:
	DetailsRefreshCoordinator(Presentation::TorrentListPresenter &torrentPresenter,
		Presentation::TorrentDetailsPresenter &detailsPresenter, DetailsModelAdapter &model,
		MainWindow &window, int &selectedTab, std::vector<Presentation::TorrentRowDto> &visibleRows);
	void refresh(AppTab activeTab);
	void reset();
	void clearSelection();
	void clearDetailsContentPreservingSelection(const std::string &id, const std::string &name, const std::string &message);

private:
	void clear(const std::string &message);
	Presentation::TorrentListPresenter &torrentPresenter_;
	Presentation::TorrentDetailsPresenter &detailsPresenter_;
	DetailsModelAdapter &model_;
	MainWindow &window_;
	int &selectedTab_;
	std::vector<Presentation::TorrentRowDto> &visibleRows_;
	std::uint64_t lastRevision_ = 0;
	std::string lastTorrentId_;
	int lastTab_ = -1;
	std::chrono::steady_clock::time_point lastRefresh_{};
};

class NotificationController
{
public:
	NotificationController(Utils::SystemUtils::SystemOpener &opener,
		Presentation::TorrentListPresenter &presenter, MainWindow &window);
	void drain();

private:
	Utils::SystemUtils::SystemOpener &opener_;
	Presentation::TorrentListPresenter &presenter_;
	MainWindow &window_;
	std::chrono::steady_clock::time_point deadline_{};
	std::string message_;
	bool targetsDetails_ = false;
};
} // namespace SlintUi

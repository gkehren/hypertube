#pragma once

#include "app/App.hpp"
#include "Result.hpp"
#include "main-window.h"
#include "presentation/TorrentListPresenter.hpp"
#include "presentation/TorrentDetailsPresenter.hpp"
#include "presentation/SearchPresenter.hpp"
#include "SlintModelAdapter.hpp"
#include "SearchModelAdapter.hpp"
#include "DetailsModelAdapter.hpp"
#include "LogModelAdapter.hpp"
#include "presentation/LogsPresenter.hpp"
#include "presentation/PreferencesController.hpp"
#include "presentation/UiStateController.hpp"
#include "presentation/TorrentAddController.hpp"
#include "DialogService.hpp"
#include "SlintControllerFacades.hpp"
#include "SlintRefreshCoordinators.hpp"

#include <slint.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

class SlintAppController
{
public:
	SlintAppController(App &app, slint::ComponentHandle<MainWindow> window);

	void bind();
	void start();
	Result stop();

private:
	void refresh();
	void autosave();
	void applyUiState(const Presentation::UiStateSnapshot &state);
	void refreshCredentialIndicators();
	static Presentation::UiStateSnapshot uiStateFrom(const PreferencesSettings &preferences);
	Presentation::UiStateSnapshot currentUiState() const;

	App &app;
	slint::ComponentHandle<MainWindow> window;
	Presentation::TorrentListPresenter torrentPresenter;
	Presentation::TorrentDetailsPresenter detailsPresenter;
	Presentation::SearchPresenter searchPresenter;
	SlintModelAdapter modelAdapter;
	SearchModelAdapter searchModelAdapter;
	SearchModelAdapter favoritesModelAdapter;
	std::shared_ptr<slint::VectorModel<CategoryRow>> categoryModel_;
	std::shared_ptr<slint::VectorModel<slint::SharedString>> recentSearchModel_;
	DetailsModelAdapter detailsModelAdapter;
	Presentation::LogsPresenter logsPresenter;
	LogModelAdapter logModelAdapter;
	Presentation::PreferencesController preferencesController;
	Presentation::UiStateController uiStateController;
	TorrentAddController addController;
	std::unique_ptr<DialogService> dialogService;
	std::unique_ptr<SlintUi::TorrentUiController> torrentUiController_;
	std::unique_ptr<SlintUi::SearchUiController> searchUiController_;
	std::unique_ptr<SlintUi::DetailsUiController> detailsUiController_;
	std::unique_ptr<SlintUi::PreferencesUiController> preferencesUiController_;
	std::unique_ptr<SlintUi::DialogCoordinator> dialogCoordinator_;
	std::unique_ptr<SlintUi::AppShellController> appShellController_;
	std::unique_ptr<SlintUi::TorrentRefreshCoordinator> torrentRefreshCoordinator_;
	std::unique_ptr<SlintUi::SearchRefreshCoordinator> searchRefreshCoordinator_;
	std::unique_ptr<SlintUi::LogRefreshCoordinator> logRefreshCoordinator_;
	std::unique_ptr<SlintUi::DetailsRefreshCoordinator> detailsRefreshCoordinator_;
	std::unique_ptr<SlintUi::NotificationController> notificationController_;
	slint::Timer refreshTimer;
	slint::Timer autosaveTimer;
	bool started = false;
	bool searchFocusRequest_ = false;
	bool torrentViewDirty_ = true;
	std::vector<Presentation::TorrentRowDto> visibleTorrentRows_;
	Presentation::TorrentSortField sortField_ = Presentation::TorrentSortField::Queue;
	bool sortAscending_ = true;
	int selectedDetailsTab_ = 0;
	std::string pendingRemoveId_;
};

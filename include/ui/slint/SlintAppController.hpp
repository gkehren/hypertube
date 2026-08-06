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
	void selectTorrent(const std::string &id);
	void executeCommand(const std::string &id, UiTorrentCommand command);
	void removeTorrent(const std::string &id);
	void confirmRemove(RemovalMode mode);
	void cancelRemove();
	void openAddDialog();
	void addSelectedSearchResult(const std::string &id);
	void submitMagnet(const std::string &magnet, const std::string &savePath);
	void submitTorrentFile(const std::string &path, const std::string &savePath);
	void cancelAdd();
	void browseTorrentFile();
	void browseSaveDirectory();
	void detailsAction(DetailsAction action);
	void previewDetailFile(int fileIndex);
	void setFilePriority(int fileIndex, int priority);
	void setSpeedLimits(const std::string &downloadLimit, const std::string &uploadLimit);
	void setSequentialDownload(bool enabled);
	void setActiveTab(AppTab tab);
	void setCategoryFilter(TorrentCategory filter);
	void setTextFilter(const std::string &filter);
	void sortTorrents(TorrentSort field);
	void executeSearch(const std::string &query);
	void cancelSearch();
	void loadMoreSearch();
	void selectSearchResult(const std::string &id);
	void toggleSearchFavorite(const std::string &id);
	void refreshSearch();
	void setSelectedDetailsTab(DetailsTab tab);
	void clearLogs();
	void setLogFilter(LogLevel level, bool enabled);
	void setLogAutoscroll(bool enabled);
	void changeTheme(Theme theme);
	void toggleSidebar();
	void browsePreferenceDirectory();
	void applyPreferences();
	void resizeLayout(int sidebarWidth, int bottomPanelHeight);
	void navigateTorrent(int direction);
	void focusSearch();
	void copyMagnet(const std::string &id);
	void selectSearchHistory(const std::string &query);
	void clearSearchHistory();
	void removeFavorite(const std::string &id);
	void clearTorznabSecret();
	void clearProxySecret();
	void showAbout();
	void applyUiState(const Presentation::UiStateSnapshot &state);
	void drainSystemOpenResults();
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
	slint::Timer refreshTimer;
	slint::Timer autosaveTimer;
	bool started = false;
	bool searchFocusRequest_ = false;
	bool torrentViewDirty_ = true;
	std::uint64_t lastStatusRevision_ = 0;
	std::uint64_t lastSearchRevision_ = 0;
	std::uint64_t lastLogRevision_ = 0;
	std::uint64_t lastDetailsRevision_ = 0;
	std::string lastDetailsTorrentId_;
	int lastDetailsTab_ = -1;
	std::chrono::steady_clock::time_point lastDetailsRefresh_{};
	std::chrono::steady_clock::time_point systemOpenMessageDeadline_{};
	std::string systemOpenMessage_;
	bool systemOpenMessageTargetsDetails_ = false;
	std::vector<Presentation::TorrentRowDto> visibleTorrentRows_;
	Presentation::TorrentSortField sortField_ = Presentation::TorrentSortField::Queue;
	bool sortAscending_ = true;
	int selectedDetailsTab_ = 0;
	std::string pendingRemoveId_;
};

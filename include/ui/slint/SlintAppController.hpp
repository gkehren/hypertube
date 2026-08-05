#pragma once

#include "app/App.hpp"
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
#include "presentation/TorrentAddController.hpp"
#include "DialogService.hpp"

#include <slint.h>

#include <chrono>
#include <memory>

class SlintAppController
{
public:
	SlintAppController(App &app, slint::ComponentHandle<MainWindow> window);

	void bind();
	void start();
	void stop();

private:
	void refresh();
	void selectTorrent(const std::string &id);
	void executeCommand(const std::string &id, int command);
	void removeTorrent(const std::string &id);
	void confirmRemove(int mode);
	void cancelRemove();
	void openAddDialog();
	void addSelectedSearchResult(const std::string &id);
	void submitMagnet(const std::string &magnet, const std::string &savePath);
	void submitTorrentFile(const std::string &path, const std::string &savePath);
	void cancelAdd();
	void browseTorrentFile();
	void browseSaveDirectory();
	void detailsAction(int action);
	void previewDetailFile(int fileIndex);
	void setFilePriority(int fileIndex, int priority);
	void setSpeedLimits(const std::string &downloadLimit, const std::string &uploadLimit);
	void setSequentialDownload(bool enabled);
	void setActiveTab(int tab);
	void setCategoryFilter(int filter);
	void setTextFilter(const std::string &filter);
	void sortTorrents(int field);
	void executeSearch(const std::string &query);
	void cancelSearch();
	void loadMoreSearch();
	void selectSearchResult(const std::string &id);
	void toggleSearchFavorite(const std::string &id);
	void refreshSearch();
	void setSelectedDetailsTab(int tab);
	void clearLogs();
	void changeTheme(int theme);
	void toggleSidebar();
	void browsePreferenceDirectory();
	void applyPreferences();
	void resizeLayout(int sidebarWidth, int bottomPanelHeight);
	void navigateTorrent(int direction);
	void focusSearch();

	App &app;
	slint::ComponentHandle<MainWindow> window;
	Presentation::TorrentListPresenter torrentPresenter;
	Presentation::TorrentDetailsPresenter detailsPresenter;
	Presentation::SearchPresenter searchPresenter;
	SlintModelAdapter modelAdapter;
	SearchModelAdapter searchModelAdapter;
	DetailsModelAdapter detailsModelAdapter;
	Presentation::LogsPresenter logsPresenter;
	LogModelAdapter logModelAdapter;
	Presentation::PreferencesController preferencesController;
	TorrentAddController addController;
	std::unique_ptr<DialogService> dialogService;
	slint::Timer refreshTimer;
	bool started = false;
	Presentation::TorrentSortField sortField_ = Presentation::TorrentSortField::Queue;
	bool sortAscending_ = true;
	int selectedDetailsTab_ = 0;
	std::string pendingRemoveId_;
};

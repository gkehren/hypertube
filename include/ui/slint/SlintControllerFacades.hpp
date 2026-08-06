#pragma once

#include "main-window.h"
#include "presentation/PreferencesController.hpp"
#include "presentation/SearchPresenter.hpp"
#include "presentation/TorrentDetailsPresenter.hpp"
#include "presentation/TorrentListPresenter.hpp"
#include "presentation/UiStateController.hpp"
#include "presentation/LogsPresenter.hpp"
#include "DialogService.hpp"
#include "LogModelAdapter.hpp"

#include <functional>
#include <string>

class App;
class TorrentAddController;

namespace SlintUi
{

class TorrentUiController
{
public:
	TorrentUiController(Presentation::TorrentListPresenter &presenter, MainWindow &window,
		Presentation::TorrentDetailsPresenter &detailsPresenter, std::function<void()> refresh,
		std::function<void()> resetDetails, Presentation::TorrentSortField &sortField, bool &sortAscending,
		bool &viewDirty, std::string &pendingRemoveId);

	void select(const std::string &id);
	void executeCommand(const std::string &id, UiTorrentCommand command);
	void remove(const std::string &id);
	void confirmRemove(RemovalMode mode);
	void cancelRemove();
	void navigate(int direction);
	void copyMagnet(const std::string &id);
	void setCategory(TorrentCategory category);
	void setTextFilter(const std::string &filter);
	void sort(TorrentSort field);

private:
	bool validateId(const std::string &id, bool allowLoading = true);
	Presentation::TorrentListPresenter &presenter_;
	MainWindow &window_;
	Presentation::TorrentDetailsPresenter &detailsPresenter_;
	std::function<void()> refresh_;
	std::function<void()> resetDetails_;
	Presentation::TorrentSortField &sortField_;
	bool &sortAscending_;
	bool &viewDirty_;
	std::string &pendingRemoveId_;
};

class SearchUiController
{
public:
	SearchUiController(Presentation::SearchPresenter &presenter, MainWindow &window,
		std::function<void()> refreshSearch);

	void execute(const std::string &query);
	void cancel();
	void loadMore();
	bool select(const std::string &id);
	void toggleFavorite(const std::string &id);
	void selectHistory(const std::string &query);
	void clearHistory();
	void removeFavorite(const std::string &id);

private:
	Presentation::SearchPresenter &presenter_;
	MainWindow &window_;
	std::function<void()> refreshSearch_;
};

class DetailsUiController
{
public:
	DetailsUiController(Presentation::TorrentDetailsPresenter &presenter, MainWindow &window,
		std::function<void()> refresh, int &selectedTab, std::function<void()> resetRefresh,
		std::function<void(int)> persistTab);

	void setTab(DetailsTab tab);
	void action(DetailsAction action);
	void previewFile(int fileIndex);
	void setFilePriority(int fileIndex, int priority);
	void setSpeedLimits(const std::string &downloadLimit, const std::string &uploadLimit);
	void setSequential(bool enabled);

private:
	Presentation::TorrentDetailsPresenter &presenter_;
	MainWindow &window_;
	std::function<void()> refresh_;
	int &selectedTab_;
	std::function<void()> resetRefresh_;
	std::function<void(int)> persistTab_;
};

class PreferencesUiController
{
public:
	PreferencesUiController(Presentation::PreferencesController &preferences,
		Presentation::UiStateController &uiState, MainWindow &window,
		std::function<Presentation::UiStateSnapshot()> currentState);

	void changeTheme(Theme theme);
	void toggleSidebar();
	void resizeLayout(int sidebarWidth, int bottomPanelHeight);
	void apply();
	void clearTorznabSecret();
	void clearProxySecret();

private:
	Presentation::PreferencesController &preferences_;
	Presentation::UiStateController &uiState_;
	MainWindow &window_;
	std::function<Presentation::UiStateSnapshot()> currentState_;
};

class DialogCoordinator
{
public:
	DialogCoordinator(App &app, TorrentAddController &addController,
		Presentation::PreferencesController &preferences, Presentation::SearchPresenter &searchPresenter,
		DialogService &dialogs, MainWindow &window, std::function<void()> refresh);

	void openAddDialog();
	void addSelectedSearchResult(const std::string &id);
	void submitMagnet(const std::string &magnet, const std::string &savePath);
	void submitTorrentFile(const std::string &path, const std::string &savePath);
	void cancelAdd();
	void browseTorrentFile();
	void browseSaveDirectory();
	void browsePreferenceDirectory();

private:
	App &app_;
	TorrentAddController &addController_;
	Presentation::PreferencesController &preferences_;
	Presentation::SearchPresenter &searchPresenter_;
	DialogService &dialogs_;
	MainWindow &window_;
	std::function<void()> refresh_;
};

class AppShellController
{
public:
	AppShellController(Presentation::LogsPresenter &logs, LogModelAdapter &logModel, MainWindow &window,
		Presentation::UiStateController &uiState, std::function<Presentation::UiStateSnapshot()> currentState,
		bool &viewDirty, std::function<void()> resetDetails, std::function<void()> refresh, bool &focusRequest);

	void setActiveTab(AppTab tab);
	void clearLogs();
	void setLogFilter(LogLevel level, bool enabled);
	void setLogAutoscroll(bool enabled);
	void focusSearch();
	void showAbout();

private:
	Presentation::LogsPresenter &logs_;
	LogModelAdapter &logModel_;
	MainWindow &window_;
	Presentation::UiStateController &uiState_;
	std::function<Presentation::UiStateSnapshot()> currentState_;
	bool &viewDirty_;
	std::function<void()> resetDetails_;
	std::function<void()> refresh_;
	bool &focusRequest_;
};
} // namespace SlintUi

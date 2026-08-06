#include "SlintAppController.hpp"

#include <algorithm>

#include "AppPaths.hpp"
#include "CredentialStore.hpp"
#include "Logger.hpp"
#include "SlintString.hpp"

Presentation::UiStateSnapshot SlintAppController::uiStateFrom(const PreferencesSettings &preferences)
{
	return {preferences.theme, preferences.ui};
}

SlintAppController::SlintAppController(App &app, slint::ComponentHandle<MainWindow> window)
	: app(app), window(std::move(window)), torrentPresenter(app.torrentManager()),
	detailsPresenter(app.torrentManager(), app.systemOpener()), searchPresenter(app.searchEngine()),
	logsPresenter(app.torrentManager()),
	preferencesController(app.torrentManager(), app.searchEngine(), app.settingsConfigManager(),
		[this](int theme) { this->window->set_selected_theme(static_cast<Theme>(std::clamp(theme, 0, 4))); }),
		uiStateController(preferencesController, uiStateFrom(preferencesController.current()),
		[this](const Presentation::UiStateSnapshot &state) { applyUiState(state); },
		[this](const Result &result) {
			this->window->set_preferences_state_message(SlintUi::toSharedString("Layout rolled back: " + result.message));
		}),
	addController(), dialogService(createDialogService()),
	categoryModel_(std::make_shared<slint::VectorModel<CategoryRow>>()),
	recentSearchModel_(std::make_shared<slint::VectorModel<slint::SharedString>>())
{
	torrentUiController_ = std::make_unique<SlintUi::TorrentUiController>(torrentPresenter, *window, detailsPresenter,
		[this] { refresh(); }, [this] { lastDetailsRefresh_ = {}; }, sortField_, sortAscending_, torrentViewDirty_,
		pendingRemoveId_);
	searchUiController_ = std::make_unique<SlintUi::SearchUiController>(searchPresenter, *window,
		[this] { refreshSearch(); });
	detailsUiController_ = std::make_unique<SlintUi::DetailsUiController>(detailsPresenter, *window,
		[this] { refresh(); }, selectedDetailsTab_, [this] { lastDetailsRefresh_ = {}; },
		[this](int tab) {
			auto state = currentUiState();
			state.layout.selectedDetailsTab = tab;
			uiStateController.request(state);
		});
	preferencesUiController_ = std::make_unique<SlintUi::PreferencesUiController>(preferencesController,
		uiStateController, *window, [this] { return currentUiState(); });
	dialogCoordinator_ = std::make_unique<SlintUi::DialogCoordinator>(app, addController, preferencesController,
		searchPresenter, *dialogService, *window, [this] { refresh(); });
	appShellController_ = std::make_unique<SlintUi::AppShellController>(logsPresenter, logModelAdapter, *window,
		uiStateController, [this] { return currentUiState(); }, torrentViewDirty_,
		[this] { lastDetailsRefresh_ = {}; }, [this] { refresh(); }, searchFocusRequest_);
}

void SlintAppController::bind()
{
	window->on_request_close([] {
		slint::quit_event_loop();
	});
	window->window().on_close_requested([] {
		slint::quit_event_loop();
		return slint::CloseRequestResponse::HideWindow;
	});
	window->on_refresh_torrents([this] { refresh(); });
	window->on_select_torrent([this](const slint::SharedString &id) {
		torrentUiController_->select(std::string(id.begin(), id.end()));
	});
	window->on_execute_torrent_command([this](const slint::SharedString &id, UiTorrentCommand command) {
		executeCommand(std::string(id.begin(), id.end()), command);
	});
	window->on_remove_torrent([this](const slint::SharedString &id) {
		torrentUiController_->remove(std::string(id.begin(), id.end()));
	});
	window->on_confirm_remove([this](RemovalMode mode) { torrentUiController_->confirmRemove(mode); });
	window->on_cancel_remove([this] { torrentUiController_->cancelRemove(); });
	window->on_torrents_tab([this] { setActiveTab(AppTab::Torrents); });
	window->on_search_tab([this] { setActiveTab(AppTab::Search); });
	window->on_favorites_tab([this] { setActiveTab(AppTab::Favorites); });
	window->on_preferences_tab([this] { setActiveTab(AppTab::Preferences); });
	window->on_logs_tab([this] { setActiveTab(AppTab::Logs); });
	window->on_select_category([this](TorrentCategory filter) { setCategoryFilter(filter); });
	window->on_filter_torrents([this](const slint::SharedString &filter) {
		setTextFilter(std::string(filter.begin(), filter.end()));
	});
	window->on_sort_torrents([this](TorrentSort field) { sortTorrents(field); });
	window->on_execute_search([this](const slint::SharedString &query) {
		executeSearch(std::string(query.begin(), query.end()));
	});
	window->on_cancel_search([this] { cancelSearch(); });
	window->on_load_more_search([this] { loadMoreSearch(); });
	window->on_select_search_result([this](const slint::SharedString &id) {
		selectSearchResult(std::string(id.begin(), id.end()));
	});
	window->on_toggle_search_favorite([this](const slint::SharedString &id) {
		toggleSearchFavorite(std::string(id.begin(), id.end()));
	});
	window->on_add_search_result([this](const slint::SharedString &id) {
		dialogCoordinator_->addSelectedSearchResult(std::string(id.begin(), id.end()));
	});
	window->on_select_details_tab([this](DetailsTab tab) { setSelectedDetailsTab(tab); });
	window->on_details_action([this](DetailsAction action) { detailsAction(action); });
	window->on_preview_detail_file([this](int index) { previewDetailFile(index); });
	window->on_set_file_priority([this](int index, int priority) { setFilePriority(index, priority); });
	window->on_set_speed_limits([this](const slint::SharedString &download, const slint::SharedString &upload) {
		setSpeedLimits(std::string(download.begin(), download.end()), std::string(upload.begin(), upload.end()));
	});
	window->on_set_sequential([this](bool enabled) { setSequentialDownload(enabled); });
	window->on_clear_logs([this] { clearLogs(); });
	window->on_set_log_filter([this](LogLevel level, bool enabled) { setLogFilter(level, enabled); });
	window->on_toggle_log_autoscroll([this](bool enabled) { setLogAutoscroll(enabled); });
	window->on_change_theme([this](Theme theme) { changeTheme(theme); });
	window->on_toggle_sidebar([this] { toggleSidebar(); });
	window->on_browse_preference_directory([this] { browsePreferenceDirectory(); });
	window->on_apply_preferences([this] { preferencesUiController_->apply(); });
	window->on_resize_layout([this](int sidebarWidth, int bottomPanelHeight) {
		resizeLayout(sidebarWidth, bottomPanelHeight);
	});
	window->on_add_torrent([this] { dialogCoordinator_->openAddDialog(); });
	window->on_submit_add_magnet([this](const slint::SharedString &magnet, const slint::SharedString &savePath) {
		dialogCoordinator_->submitMagnet(std::string(magnet.begin(), magnet.end()), std::string(savePath.begin(), savePath.end()));
	});
	window->on_submit_add_file([this](const slint::SharedString &path, const slint::SharedString &savePath) {
		dialogCoordinator_->submitTorrentFile(std::string(path.begin(), path.end()), std::string(savePath.begin(), savePath.end()));
	});
	window->on_cancel_add([this] { dialogCoordinator_->cancelAdd(); });
	window->on_browse_torrent_file([this] { browseTorrentFile(); });
	window->on_browse_save_directory([this] { browseSaveDirectory(); });
	window->on_navigate_torrent([this](int direction) { torrentUiController_->navigate(direction); });
	window->on_focus_search([this] { focusSearch(); });
	window->on_copy_magnet([this](const slint::SharedString &id) { torrentUiController_->copyMagnet(std::string(id.begin(), id.end())); });
	window->on_select_search_history([this](const slint::SharedString &query) {
		searchUiController_->selectHistory(std::string(query.begin(), query.end()));
	});
	window->on_clear_search_history([this] { searchUiController_->clearHistory(); });
	window->on_remove_favorite([this](const slint::SharedString &id) {
		searchUiController_->removeFavorite(std::string(id.begin(), id.end()));
	});
	window->on_clear_torznab_secret([this] { preferencesUiController_->clearTorznabSecret(); });
	window->on_clear_proxy_secret([this] { preferencesUiController_->clearProxySecret(); });
	window->on_show_about([this] { appShellController_->showAbout(); });
	window->on_cancel_dialogs([this] {
		dialogCoordinator_->cancelAdd();
		torrentUiController_->cancelRemove();
	});
}

void SlintAppController::start()
{
	const auto preferences = app.settingsConfigManager().getPreferencesSettings();
	window->set_version_text(slint::SharedString("Slint v1.16.1"));
	window->set_startup_state(slint::SharedString("Starting"));
	window->set_sidebar_width(preferences.ui.sidebarWidth);
	window->set_bottom_panel_height(preferences.ui.bottomPanelHeight);
	window->set_sidebar_collapsed(preferences.ui.sidebarCollapsed);
	window->set_active_tab(static_cast<AppTab>(std::clamp(preferences.ui.selectedMainTab, 0, 4)));
	window->set_category_filter(static_cast<TorrentCategory>(torrentPresenter.categoryFilter()));
	window->set_categories(categoryModel_);
	window->set_torrent_rows(modelAdapter.model());
	window->set_search_rows(searchModelAdapter.model());
	window->set_favorite_rows(favoritesModelAdapter.model());
	window->set_detail_files(detailsModelAdapter.filesModel());
	window->set_detail_peers(detailsModelAdapter.peersModel());
	window->set_detail_trackers(detailsModelAdapter.trackersModel());
	window->set_log_rows(logModelAdapter.model());
	window->set_log_show_debug(logsPresenter.levelEnabled(Utils::LogLevel::Debug));
	window->set_log_show_info(logsPresenter.levelEnabled(Utils::LogLevel::Info));
	window->set_log_show_warning(logsPresenter.levelEnabled(Utils::LogLevel::Warning));
	window->set_log_show_error(logsPresenter.levelEnabled(Utils::LogLevel::Error));
	window->set_log_auto_scroll(true);
	window->set_logs_state_message(slint::SharedString("Diagnostics are updated from the bounded log buffer."));
	const auto currentPreferences = preferencesController.current();
	selectedDetailsTab_ = std::clamp(currentPreferences.ui.selectedDetailsTab, 0, 4);
	window->set_selected_theme(static_cast<Theme>(std::clamp(currentPreferences.theme, 0, 4)));
	window->set_preferences_state_message(slint::SharedString("Changes are saved transactionally."));
	window->set_preference_download_limit(SlintUi::toSharedString(std::to_string(currentPreferences.downloadSpeedLimit)));
	window->set_preference_upload_limit(SlintUi::toSharedString(std::to_string(currentPreferences.uploadSpeedLimit)));
	window->set_preference_download_path(SlintUi::toSharedString(currentPreferences.downloadPath));
	window->set_preference_enable_dht(currentPreferences.enableDht);
	window->set_preference_enable_upnp(currentPreferences.enableUpnp);
	window->set_preference_enable_natpmp(currentPreferences.enableNatPmp);
	window->set_preference_torznab_enabled(currentPreferences.torznabEnabled);
	window->set_preference_torznab_url(SlintUi::toSharedString(currentPreferences.torznabUrl));
	window->set_preference_torznab_secret_stored(Utils::CredentialStore::load("torznab_api_key").has_value());
	window->set_preference_torznab_secret(slint::SharedString());
	window->set_preference_proxy_enabled(currentPreferences.proxyEnabled);
	window->set_preference_proxy_type(SlintUi::toSharedString(currentPreferences.proxyType));
	window->set_preference_proxy_host(SlintUi::toSharedString(currentPreferences.proxyHost));
	window->set_preference_proxy_port(SlintUi::toSharedString(std::to_string(currentPreferences.proxyPort)));
	window->set_preference_proxy_username(SlintUi::toSharedString(currentPreferences.proxyUsername));
	window->set_preference_proxy_secret_stored(Utils::CredentialStore::load("proxy_password").has_value());
	window->set_preference_proxy_secret(slint::SharedString());
	window->set_preference_clear_torznab_secret(false);
	window->set_preference_clear_proxy_secret(false);
	window->set_selected_details_tab(static_cast<DetailsTab>(selectedDetailsTab_));
	window->set_add_dialog_open(false);
	window->set_remove_dialog_open(false);
	window->set_search_query(slint::SharedString());
	started = true;
	app.torrentManager().requestStatusRefresh();
	refreshSearch();
	refresh();
	drainSystemOpenResults();
	refreshTimer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(250), [this] {
		refresh();
		drainSystemOpenResults();
	});
	autosaveTimer.start(slint::TimerMode::Repeated, std::chrono::seconds(30), [this] { autosave(); });
}

Result SlintAppController::stop()
{
	refreshTimer.stop();
	autosaveTimer.stop();
	started = false;
	Result result = uiStateController.flush();
	const Result preferences = preferencesController.waitForSave();
	if (!preferences)
		result = preferences;
	if (uiStateController.hasPending())
	{
		const Result finalUi = uiStateController.flush();
		if (!finalUi)
			result = finalUi;
	}
	if (!result)
		Utils::Logger::warning("app", "Slint preferences could not be finalized: " + result.message);
	return result;
}

void SlintAppController::refresh()
{
	app.torrentManager().requestStatusRefresh();
	const auto activeTab = window->get_active_tab();
	const bool searchVisible = activeTab == AppTab::Search || activeTab == AppTab::Favorites;
	if (searchVisible || searchPresenter.isSearching())
	{
		searchPresenter.update();
		if (searchPresenter.revision() != lastSearchRevision_ || searchPresenter.isSearching())
			refreshSearch();
	}
	if (uiStateController.hasPending())
	{
		uiStateController.poll();
		if (preferencesController.lastCompletedSaveKind() == Presentation::PreferencesController::SaveKind::Preferences)
			refreshCredentialIndicators();
	}
	else if (const auto saveResult = preferencesController.pollSave())
	{
		if (preferencesController.lastCompletedSaveKind() == Presentation::PreferencesController::SaveKind::Preferences)
			refreshCredentialIndicators();
		window->set_preferences_state_message(SlintUi::toSharedString(saveResult->success
			? "Preferences saved" : saveResult->message));
	}
	logsPresenter.update();
	if (activeTab == AppTab::Logs && Utils::Logger::revision() != lastLogRevision_)
	{
		logModelAdapter.update(logsPresenter.buildRows());
		window->set_log_rows(logModelAdapter.model());
		lastLogRevision_ = Utils::Logger::revision();
	}

	const auto statusRevision = app.torrentManager().getStatusRevision();
	const bool statusChanged = statusRevision != lastStatusRevision_;
	if (statusChanged || torrentViewDirty_)
	{
		while (categoryModel_->row_count() > 0)
			categoryModel_->erase(categoryModel_->row_count() - 1);
		for (const auto &category : torrentPresenter.buildCategories())
			categoryModel_->push_back(CategoryRow{static_cast<TorrentCategory>(category.id), SlintUi::toSharedString(category.label), category.count});
		window->set_categories(categoryModel_);
		if (activeTab == AppTab::Torrents)
		{
			visibleTorrentRows_ = torrentPresenter.buildRows();
			modelAdapter.update(visibleTorrentRows_);
			window->set_torrent_rows(modelAdapter.model());
		}
		torrentViewDirty_ = false;
		lastStatusRevision_ = statusRevision;
	}
	const auto torrentSnapshot = app.torrentManager().getTorrentSnapshot();
	const auto statusCache = app.torrentManager().getStatusCache();
	const bool waitingForStatus = !torrentSnapshot.empty() && (!statusCache || statusCache->empty());
	window->set_startup_state(slint::SharedString(waitingForStatus ? "Loading torrent statuses..." : "Ready"));
	if (activeTab != AppTab::Torrents)
		return;

	const auto now = std::chrono::steady_clock::now();
	const auto detailsInterval = selectedDetailsTab_ == 2 || selectedDetailsTab_ == 3
		? std::chrono::seconds(2) : std::chrono::milliseconds(500);
	const bool detailsDue = lastDetailsRefresh_ == std::chrono::steady_clock::time_point{}
		|| now - lastDetailsRefresh_ >= detailsInterval;
	if (!detailsDue)
		return;
	lastDetailsRefresh_ = now;

	if (torrentPresenter.selectedId().empty())
	{
		lastDetailsRevision_ = 0;
		lastDetailsTorrentId_.clear();
		lastDetailsTab_ = -1;
		window->set_selected_torrent_id(slint::SharedString());
		window->set_selected_torrent_name(slint::SharedString());
		window->set_selected_torrent_state(slint::SharedString());
		window->set_selected_torrent_progress(slint::SharedString());
		window->set_selected_torrent_size(slint::SharedString());
		window->set_selected_torrent_down_rate(slint::SharedString());
		window->set_selected_torrent_up_rate(slint::SharedString());
		window->set_selected_torrent_eta(slint::SharedString());
		window->set_selected_torrent_save_path(slint::SharedString());
		window->set_selected_download_limit(slint::SharedString());
		window->set_selected_upload_limit(slint::SharedString());
		window->set_selected_torrent_paused(false);
		window->set_selected_sequential_download(false);
		window->set_details_message(slint::SharedString("Select a torrent"));
		detailsModelAdapter.updateFiles({});
		detailsModelAdapter.updatePeers({});
		detailsModelAdapter.updateTrackers({});
		return;
	}

	const std::string selectedId = torrentPresenter.selectedId();
	const Presentation::TorrentRowDto *selectedRow = nullptr;
	for (const auto &row : visibleTorrentRows_)
	{
		if (row.id == selectedId)
		{
			selectedRow = &row;
			break;
		}
	}

	// The visible model is intentionally refreshed only on a status revision,
	// filter, sort, or tab change. Resolve a selection against the unfiltered
	// presenter when that cache is stale or the selected row is hidden by the
	// current filter.
	std::optional<Presentation::TorrentRowDto> resolvedRow;
	if (!selectedRow)
	{
		resolvedRow = torrentPresenter.findRowById(selectedId);
		if (resolvedRow)
			selectedRow = &*resolvedRow;
	}

	if (selectedRow)
	{
		const auto hash = torrentPresenter.hashForId(selectedRow->id);
		detailsPresenter.setSelectedTorrent(hash);
		detailsPresenter.setSelectedTab(static_cast<Presentation::DetailsTab>(selectedDetailsTab_));
		const auto details = detailsPresenter.buildGeneral();
		window->set_selected_torrent_id(SlintUi::toSharedString(selectedRow->id));
		window->set_selected_torrent_name(SlintUi::toSharedString(selectedRow->name));
		window->set_selected_torrent_paused(selectedRow->paused);
			if (details)
			{
				window->set_selected_torrent_state(SlintUi::toSharedString(details->stateLabel));
				window->set_selected_torrent_progress(SlintUi::toSharedString(details->progressLabel));
				window->set_selected_torrent_size(SlintUi::toSharedString(details->sizeLabel));
				window->set_selected_torrent_down_rate(SlintUi::toSharedString(details->downloadRateLabel));
				window->set_selected_torrent_up_rate(SlintUi::toSharedString(details->uploadRateLabel));
				window->set_selected_torrent_eta(SlintUi::toSharedString(details->etaLabel));
				window->set_selected_torrent_save_path(SlintUi::toSharedString(details->savePath));
				if (const auto settings = detailsPresenter.buildSettings())
				{
					window->set_selected_sequential_download(settings->sequentialDownload);
					window->set_selected_download_limit(SlintUi::toSharedString(std::to_string(settings->downloadLimitBytes)));
					window->set_selected_upload_limit(SlintUi::toSharedString(std::to_string(settings->uploadLimitBytes)));
				}
				else
				{
					window->set_selected_sequential_download(false);
					window->set_selected_download_limit(slint::SharedString());
					window->set_selected_upload_limit(slint::SharedString());
				}
				window->set_details_message(slint::SharedString("General"));
				if (selectedDetailsTab_ == 1 || selectedDetailsTab_ == 2 || selectedDetailsTab_ == 3)
				{
					const auto section = detailsPresenter.buildSection(detailsPresenter.selectedTab());
					const bool detailsChanged = lastDetailsTorrentId_ != selectedRow->id
						|| lastDetailsTab_ != selectedDetailsTab_ || lastDetailsRevision_ != section.revision;
					if (selectedDetailsTab_ == 1)
					{
						if (detailsChanged)
							detailsModelAdapter.updateFiles(section.files);
					}
					else if (selectedDetailsTab_ == 2)
					{
						if (detailsChanged)
							detailsModelAdapter.updatePeers(section.peers);
					}
					else
					{
						if (detailsChanged)
							detailsModelAdapter.updateTrackers(section.trackers);
					}
					lastDetailsRevision_ = section.revision;
					lastDetailsTorrentId_ = selectedRow->id;
					lastDetailsTab_ = selectedDetailsTab_;
					if (!section.message.empty())
						window->set_details_message(SlintUi::toSharedString(section.message));
					else if (section.state == Presentation::DetailsState::Loading)
						window->set_details_message(slint::SharedString("Loading details..."));
					else if (section.files.empty() && section.peers.empty() && section.trackers.empty())
						window->set_details_message(slint::SharedString("No details available"));
				}
				else if (selectedDetailsTab_ == 4)
				{
					const auto settings = detailsPresenter.buildSettings();
					window->set_selected_sequential_download(settings && settings->sequentialDownload);
					window->set_details_message(slint::SharedString(settings ? "Settings ready" : "Settings unavailable"));
				}
			}
			else
				window->set_details_message(slint::SharedString("Details are not available yet"));
		return;
	}

	window->set_selected_torrent_id(slint::SharedString());
	window->set_selected_torrent_name(slint::SharedString());
	window->set_selected_torrent_paused(false);
	window->set_selected_sequential_download(false);
	window->set_selected_download_limit(slint::SharedString());
	window->set_selected_upload_limit(slint::SharedString());
	window->set_details_message(slint::SharedString("The selected torrent is no longer available"));
	detailsModelAdapter.updateFiles({});
	detailsModelAdapter.updatePeers({});
	detailsModelAdapter.updateTrackers({});
	lastDetailsRevision_ = 0;
	lastDetailsTorrentId_.clear();
	lastDetailsTab_ = -1;
}

void SlintAppController::drainSystemOpenResults()
{
	for (const auto &operation : app.systemOpener().drainResults())
	{
		systemOpenMessage_ = operation.result
			? (operation.kind == Utils::SystemUtils::OpenOperationKind::Explorer
				? "Folder opened successfully" : "Preview opened successfully")
			: operation.result.message;
		systemOpenMessageTargetsDetails_ = !torrentPresenter.selectedId().empty();
		systemOpenMessageDeadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	}

	if (systemOpenMessage_.empty())
		return;
	if (std::chrono::steady_clock::now() >= systemOpenMessageDeadline_)
	{
		systemOpenMessage_.clear();
		return;
	}
	if (systemOpenMessageTargetsDetails_)
		window->set_details_message(SlintUi::toSharedString(systemOpenMessage_));
	else
		window->set_startup_state(SlintUi::toSharedString(systemOpenMessage_));
}

void SlintAppController::refreshCredentialIndicators()
{
	window->set_preference_torznab_secret_stored(
		Utils::CredentialStore::load("torznab_api_key").has_value());
	window->set_preference_proxy_secret_stored(
		Utils::CredentialStore::load("proxy_password").has_value());
}

void SlintAppController::autosave()
{
	if (!started)
		return;

	app.torrentsConfigManager().saveTorrents(app.torrentManager().getTorrentSnapshot());
	if (preferencesController.isSaving() || uiStateController.hasPending())
	{
		window->set_preferences_state_message(slint::SharedString(
			"Preferences are still being saved; settings autosave deferred."));
		return;
	}

	app.searchEngine().saveFavoritesAndHistory(app.settingsConfigManager());
	app.settingsConfigManager().save(Utils::AppPaths::settingsConfigPath().string());
}

void SlintAppController::refreshSearch()
{
	searchModelAdapter.update(searchPresenter.buildResultRows());
	window->set_search_rows(searchModelAdapter.model());
	favoritesModelAdapter.update(searchPresenter.buildResultRows(true));
	window->set_favorite_rows(favoritesModelAdapter.model());
	while (recentSearchModel_->row_count() > 0)
		recentSearchModel_->erase(recentSearchModel_->row_count() - 1);
	const auto history = app.searchEngine().getSearchHistory();
	for (std::size_t index = 0; index < history.size() && index < 5; ++index)
		recentSearchModel_->push_back(SlintUi::toSharedString(history[index]));
	window->set_recent_searches(recentSearchModel_);
	window->set_search_loading(searchPresenter.isSearching());
	window->set_search_can_load_more(searchPresenter.hasMore() && !searchPresenter.query().empty());
	std::string message = searchPresenter.stateMessage();
	if (message.empty() && searchPresenter.isSearching())
		message = "Searching...";
	window->set_search_state_message(SlintUi::toSharedString(message));
	window->set_favorites_state_message(SlintUi::toSharedString(
		searchPresenter.favorites().empty() ? "Save search results here for quick access." : "Saved search results"));
	lastSearchRevision_ = searchPresenter.revision();
}

void SlintAppController::selectTorrent(const std::string &id)
{
	torrentUiController_->select(id);
}

void SlintAppController::executeCommand(const std::string &id, UiTorrentCommand command)
{
	torrentUiController_->executeCommand(id, command);
}

void SlintAppController::removeTorrent(const std::string &id)
{
	torrentUiController_->remove(id);
}

void SlintAppController::confirmRemove(RemovalMode mode)
{
	torrentUiController_->confirmRemove(mode);
}

void SlintAppController::cancelRemove()
{
	torrentUiController_->cancelRemove();
}

void SlintAppController::openAddDialog()
{
	dialogCoordinator_->openAddDialog();
}

void SlintAppController::addSelectedSearchResult(const std::string &id)
{
	dialogCoordinator_->addSelectedSearchResult(id);
}

void SlintAppController::submitMagnet(const std::string &magnet, const std::string &savePath)
{
	dialogCoordinator_->submitMagnet(magnet, savePath);
}

void SlintAppController::submitTorrentFile(const std::string &path, const std::string &savePath)
{
	dialogCoordinator_->submitTorrentFile(path, savePath);
}

void SlintAppController::cancelAdd()
{
	dialogCoordinator_->cancelAdd();
}

void SlintAppController::browseTorrentFile()
{
	dialogCoordinator_->browseTorrentFile();
}

void SlintAppController::browseSaveDirectory()
{
	dialogCoordinator_->browseSaveDirectory();
}

void SlintAppController::setActiveTab(AppTab tab)
{
	appShellController_->setActiveTab(tab);
}

Presentation::UiStateSnapshot SlintAppController::currentUiState() const
{
	Presentation::UiStateSnapshot state = uiStateFrom(preferencesController.current());
	state.theme = static_cast<int>(window->get_selected_theme());
	state.layout.sidebarWidth = window->get_sidebar_width();
	state.layout.bottomPanelHeight = window->get_bottom_panel_height();
	state.layout.sidebarCollapsed = window->get_sidebar_collapsed();
	state.layout.selectedMainTab = static_cast<int>(window->get_active_tab());
	state.layout.selectedDetailsTab = selectedDetailsTab_;
	return state;
}

void SlintAppController::setCategoryFilter(TorrentCategory filter)
{
	torrentUiController_->setCategory(filter);
}

void SlintAppController::setTextFilter(const std::string &filter)
{
	torrentUiController_->setTextFilter(filter);
}

void SlintAppController::sortTorrents(TorrentSort field)
{
	torrentUiController_->sort(field);
}

void SlintAppController::executeSearch(const std::string &query)
{
	searchUiController_->execute(query);
}

void SlintAppController::cancelSearch()
{
	searchUiController_->cancel();
}

void SlintAppController::loadMoreSearch()
{
	searchUiController_->loadMore();
}

void SlintAppController::selectSearchResult(const std::string &id)
{
	if (searchUiController_->select(id))
		window->set_search_state_message(slint::SharedString("Result selected; choose Add torrent to continue."));
}

void SlintAppController::toggleSearchFavorite(const std::string &id)
{
	searchUiController_->toggleFavorite(id);
}

void SlintAppController::setSelectedDetailsTab(DetailsTab tab)
{
	detailsUiController_->setTab(tab);
}

void SlintAppController::detailsAction(DetailsAction action)
{
	detailsUiController_->action(action);
}

void SlintAppController::previewDetailFile(int fileIndex)
{
	detailsUiController_->previewFile(fileIndex);
}

void SlintAppController::setFilePriority(int fileIndex, int priority)
{
	detailsUiController_->setFilePriority(fileIndex, priority);
}

void SlintAppController::setSpeedLimits(const std::string &downloadLimit, const std::string &uploadLimit)
{
	detailsUiController_->setSpeedLimits(downloadLimit, uploadLimit);
}

void SlintAppController::setSequentialDownload(bool enabled)
{
	detailsUiController_->setSequential(enabled);
}

void SlintAppController::clearLogs()
{
	appShellController_->clearLogs();
}

void SlintAppController::changeTheme(Theme theme)
{
	preferencesUiController_->changeTheme(theme);
}

void SlintAppController::toggleSidebar()
{
	preferencesUiController_->toggleSidebar();
}

void SlintAppController::browsePreferenceDirectory()
{
	dialogCoordinator_->browsePreferenceDirectory();
}

void SlintAppController::applyPreferences()
{
	preferencesUiController_->apply();
}

void SlintAppController::resizeLayout(int sidebarWidth, int bottomPanelHeight)
{
	preferencesUiController_->resizeLayout(sidebarWidth, bottomPanelHeight);
}

void SlintAppController::navigateTorrent(int direction)
{
	torrentUiController_->navigate(direction);
}

void SlintAppController::focusSearch()
{
	appShellController_->focusSearch();
}

void SlintAppController::setLogFilter(LogLevel level, bool enabled)
{
	appShellController_->setLogFilter(level, enabled);
}

void SlintAppController::setLogAutoscroll(bool enabled)
{
	appShellController_->setLogAutoscroll(enabled);
}

void SlintAppController::applyUiState(const Presentation::UiStateSnapshot &state)
{
	window->set_selected_theme(static_cast<Theme>(std::clamp(state.theme, 0, 4)));
	window->set_sidebar_width(state.layout.sidebarWidth);
	window->set_bottom_panel_height(state.layout.bottomPanelHeight);
	window->set_sidebar_collapsed(state.layout.sidebarCollapsed);
	window->set_active_tab(static_cast<AppTab>(std::clamp(state.layout.selectedMainTab, 0, 4)));
	selectedDetailsTab_ = std::clamp(state.layout.selectedDetailsTab, 0, 4);
	window->set_selected_details_tab(static_cast<DetailsTab>(selectedDetailsTab_));
}

void SlintAppController::copyMagnet(const std::string &id)
{
	torrentUiController_->copyMagnet(id);
}

void SlintAppController::selectSearchHistory(const std::string &query)
{
	searchUiController_->selectHistory(query);
}

void SlintAppController::clearSearchHistory()
{
	searchUiController_->clearHistory();
}

void SlintAppController::removeFavorite(const std::string &id)
{
	searchUiController_->removeFavorite(id);
}

void SlintAppController::clearTorznabSecret()
{
	preferencesUiController_->clearTorznabSecret();
}

void SlintAppController::clearProxySecret()
{
	preferencesUiController_->clearProxySecret();
}

void SlintAppController::showAbout()
{
	appShellController_->showAbout();
}

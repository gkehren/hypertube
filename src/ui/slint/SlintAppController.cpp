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
		[this] { refresh(); }, [this] { if (detailsRefreshCoordinator_) detailsRefreshCoordinator_->reset(); }, sortField_, sortAscending_, torrentViewDirty_,
		pendingRemoveId_);
	searchUiController_ = std::make_unique<SlintUi::SearchUiController>(searchPresenter, *window,
		[this] { if (searchRefreshCoordinator_) searchRefreshCoordinator_->forceRefresh(); });
	detailsUiController_ = std::make_unique<SlintUi::DetailsUiController>(detailsPresenter, *window,
		[this] { refresh(); }, selectedDetailsTab_, [this] { if (detailsRefreshCoordinator_) detailsRefreshCoordinator_->reset(); },
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
		[this] { if (detailsRefreshCoordinator_) detailsRefreshCoordinator_->reset(); }, [this] { refresh(); }, searchFocusRequest_);
	torrentRefreshCoordinator_ = std::make_unique<SlintUi::TorrentRefreshCoordinator>(app.torrentManager(),
		torrentPresenter, modelAdapter, categoryModel_, *window, torrentViewDirty_, visibleTorrentRows_);
	searchRefreshCoordinator_ = std::make_unique<SlintUi::SearchRefreshCoordinator>(searchPresenter,
		app.searchEngine(), searchModelAdapter, favoritesModelAdapter, recentSearchModel_, *window);
	logRefreshCoordinator_ = std::make_unique<SlintUi::LogRefreshCoordinator>(logsPresenter, logModelAdapter, *window);
	detailsRefreshCoordinator_ = std::make_unique<SlintUi::DetailsRefreshCoordinator>(torrentPresenter,
		detailsPresenter, detailsModelAdapter, *window, selectedDetailsTab_, visibleTorrentRows_);
	notificationController_ = std::make_unique<SlintUi::NotificationController>(app.systemOpener(), torrentPresenter, *window);
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
		torrentUiController_->executeCommand(std::string(id.begin(), id.end()), command);
	});
	window->on_remove_torrent([this](const slint::SharedString &id) {
		torrentUiController_->remove(std::string(id.begin(), id.end()));
	});
	window->on_confirm_remove([this](RemovalMode mode) { torrentUiController_->confirmRemove(mode); });
	window->on_cancel_remove([this] { torrentUiController_->cancelRemove(); });
	window->on_torrents_tab([this] { appShellController_->setActiveTab(AppTab::Torrents); });
	window->on_search_tab([this] { appShellController_->setActiveTab(AppTab::Search); });
	window->on_favorites_tab([this] { appShellController_->setActiveTab(AppTab::Favorites); });
	window->on_preferences_tab([this] { appShellController_->setActiveTab(AppTab::Preferences); });
	window->on_logs_tab([this] { appShellController_->setActiveTab(AppTab::Logs); });
	window->on_select_category([this](TorrentCategory filter) { torrentUiController_->setCategory(filter); });
	window->on_filter_torrents([this](const slint::SharedString &filter) {
		torrentUiController_->setTextFilter(std::string(filter.begin(), filter.end()));
	});
	window->on_sort_torrents([this](TorrentSort field) { torrentUiController_->sort(field); });
	window->on_execute_search([this](const slint::SharedString &query) {
		searchUiController_->execute(std::string(query.begin(), query.end()));
	});
	window->on_cancel_search([this] { searchUiController_->cancel(); });
	window->on_load_more_search([this] { searchUiController_->loadMore(); });
	window->on_select_search_result([this](const slint::SharedString &id) {
		if (searchUiController_->select(std::string(id.begin(), id.end())))
			window->set_search_state_message(slint::SharedString("Result selected; choose Add torrent to continue."));
	});
	window->on_toggle_search_favorite([this](const slint::SharedString &id) {
		searchUiController_->toggleFavorite(std::string(id.begin(), id.end()));
	});
	window->on_add_search_result([this](const slint::SharedString &id) {
		dialogCoordinator_->addSelectedSearchResult(std::string(id.begin(), id.end()));
	});
	window->on_select_details_tab([this](DetailsTab tab) { detailsUiController_->setTab(tab); });
	window->on_details_action([this](DetailsAction action) { detailsUiController_->action(action); });
	window->on_preview_detail_file([this](int index) { detailsUiController_->previewFile(index); });
	window->on_set_file_priority([this](int index, int priority) { detailsUiController_->setFilePriority(index, priority); });
	window->on_set_speed_limits([this](const slint::SharedString &download, const slint::SharedString &upload) {
		detailsUiController_->setSpeedLimits(std::string(download.begin(), download.end()), std::string(upload.begin(), upload.end()));
	});
	window->on_set_sequential([this](bool enabled) { detailsUiController_->setSequential(enabled); });
	window->on_clear_logs([this] { appShellController_->clearLogs(); });
	window->on_export_diagnostics([this] { appShellController_->exportDiagnostics(); });
	window->on_set_log_filter([this](LogLevel level, bool enabled) { appShellController_->setLogFilter(level, enabled); });
	window->on_toggle_log_autoscroll([this](bool enabled) { appShellController_->setLogAutoscroll(enabled); });
	window->on_change_theme([this](Theme theme) { preferencesUiController_->changeTheme(theme); });
	window->on_toggle_sidebar([this] { preferencesUiController_->toggleSidebar(); });
	window->on_browse_preference_directory([this] { dialogCoordinator_->browsePreferenceDirectory(); });
	window->on_apply_preferences([this] {
		preferencesUiController_->apply();
		window->set_preferences_saving(preferencesController.isSaving());
	});
	window->on_resize_layout([this](int sidebarWidth, int bottomPanelHeight) {
		preferencesUiController_->resizeLayout(sidebarWidth, bottomPanelHeight);
	});
	window->on_add_torrent([this] { dialogCoordinator_->openAddDialog(); });
	window->on_submit_add_magnet([this](const slint::SharedString &magnet, const slint::SharedString &savePath) {
		dialogCoordinator_->submitMagnet(std::string(magnet.begin(), magnet.end()), std::string(savePath.begin(), savePath.end()));
	});
	window->on_submit_add_file([this](const slint::SharedString &path, const slint::SharedString &savePath) {
		dialogCoordinator_->submitTorrentFile(std::string(path.begin(), path.end()), std::string(savePath.begin(), savePath.end()));
	});
	window->on_cancel_add([this] { dialogCoordinator_->cancelAdd(); });
	window->on_browse_torrent_file([this] { dialogCoordinator_->browseTorrentFile(); });
	window->on_browse_save_directory([this] { dialogCoordinator_->browseSaveDirectory(); });
	window->on_navigate_torrent([this](int direction) { torrentUiController_->navigate(direction); });
	window->on_focus_search([this] { appShellController_->focusSearch(); });
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
	window->set_version_text(slint::SharedString("Slint v1.17.1"));
	window->set_startup_state(slint::SharedString("Starting"));
	window->set_sidebar_width(std::clamp(preferences.ui.sidebarWidth, 120, 600));
	window->set_bottom_panel_height(std::clamp(preferences.ui.bottomPanelHeight, 120, 600));
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
	window->set_preference_torznab_secret_stored(Utils::CredentialStore::hasStoredCredential("torznab_api_key"));
	window->set_preference_torznab_secret(slint::SharedString());
	window->set_preference_proxy_enabled(currentPreferences.proxyEnabled);
	window->set_preference_proxy_type(SlintUi::toSharedString(currentPreferences.proxyType));
	window->set_preference_proxy_host(SlintUi::toSharedString(currentPreferences.proxyHost));
	window->set_preference_proxy_port(SlintUi::toSharedString(std::to_string(currentPreferences.proxyPort)));
	window->set_preference_proxy_username(SlintUi::toSharedString(currentPreferences.proxyUsername));
	window->set_preference_proxy_secret_stored(Utils::CredentialStore::hasStoredCredential("proxy_password"));
	window->set_preference_proxy_secret(slint::SharedString());
	window->set_preference_clear_torznab_secret(false);
	window->set_preference_clear_proxy_secret(false);
	window->set_selected_details_tab(static_cast<DetailsTab>(selectedDetailsTab_));
	window->set_add_dialog_open(false);
	window->set_remove_dialog_open(false);
	window->set_search_query(slint::SharedString());
	started = true;
	Utils::CredentialStore::asyncRefreshStatus({"torznab_api_key", "proxy_password"}, [this]() {
		slint::invoke_from_event_loop([this]() {
			refreshCredentialIndicators();
		});
	});
	app.torrentManager().requestStatusRefresh();
	searchRefreshCoordinator_->forceRefresh();
	refresh();
	notificationController_->drain();
	refreshTimer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(250), [this] {
		refresh();
		notificationController_->drain();
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
	window->set_preferences_saving(preferencesController.isSaving());
	const auto activeTab = window->get_active_tab();
	searchRefreshCoordinator_->refreshIfNeeded(activeTab);
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

	if (const auto snapshotRes = app.torrentManager().pollPersistenceSnapshot())
	{
		if (snapshotRes->success)
		{
			app.torrentsConfigManager().saveTorrents(snapshotRes->torrents);
		}
		else
		{
			Utils::Logger::warning("app", "Autosave fast-resume snapshot failed: " + snapshotRes->errorMessage);
		}
	}

	window->set_preferences_saving(preferencesController.isSaving());
	logRefreshCoordinator_->refresh(activeTab);
	torrentRefreshCoordinator_->refresh(activeTab);
	detailsRefreshCoordinator_->refresh(activeTab);
}
void SlintAppController::refreshCredentialIndicators()
{
	window->set_preference_torznab_secret_stored(
		Utils::CredentialStore::hasStoredCredential("torznab_api_key"));
	window->set_preference_proxy_secret_stored(
		Utils::CredentialStore::hasStoredCredential("proxy_password"));
}

void SlintAppController::autosave()
{
	if (!started)
		return;

	app.torrentManager().requestPersistenceSnapshot();
	if (preferencesController.isSaving() || uiStateController.hasPending())
	{
		window->set_preferences_state_message(slint::SharedString(
			"Preferences are still being saved; settings autosave deferred."));
		return;
	}

	app.searchEngine().saveFavoritesAndHistory(app.settingsConfigManager());
	app.settingsConfigManager().save(Utils::AppPaths::settingsConfigPath().string());
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

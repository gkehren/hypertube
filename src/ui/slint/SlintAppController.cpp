#include "SlintAppController.hpp"

#include <algorithm>
#include <charconv>
#include <limits>

namespace
{
Result parseSpeedLimit(const std::string &value, int &output)
{
	if (value.empty())
		return Result::Failure("A speed limit is required; use 0 for unlimited", ResultCode::InvalidInput);

	long long parsed = 0;
	const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
	if (error != std::errc{} || end != value.data() + value.size()
		|| parsed < 0 || parsed > std::numeric_limits<int>::max())
		return Result::Failure("Speed limits must be whole numbers between 0 and 2147483647", ResultCode::InvalidInput);
	output = static_cast<int>(parsed);
	return Result::Success();
}

Result parseProxyPort(const std::string &value, int &output)
{
	long long parsed = 0;
	const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
	if (error != std::errc{} || end != value.data() + value.size() || parsed < 1 || parsed > 65535)
		return Result::Failure("Proxy port must be a whole number between 1 and 65535", ResultCode::InvalidInput);
	output = static_cast<int>(parsed);
	return Result::Success();
}
}

SlintAppController::SlintAppController(App &app, slint::ComponentHandle<MainWindow> window)
	: app(app), window(std::move(window)), torrentPresenter(app.torrentManager()),
	detailsPresenter(app.torrentManager(), app.systemOpener()), searchPresenter(app.searchEngine()),
	logsPresenter(app.torrentManager()),
	preferencesController(app.torrentManager(), app.searchEngine(), app.settingsConfigManager()),
	addController(), dialogService(createDialogService())
{
}

void SlintAppController::bind()
{
	window->on_request_close([] {
		slint::quit_event_loop();
	});
	window->on_refresh_torrents([this] { refresh(); });
	window->on_select_torrent([this](const slint::SharedString &id) {
		selectTorrent(std::string(id.begin(), id.end()));
	});
	window->on_execute_torrent_command([this](const slint::SharedString &id, int command) {
		executeCommand(std::string(id.begin(), id.end()), command);
	});
	window->on_remove_torrent([this](const slint::SharedString &id) {
		removeTorrent(std::string(id.begin(), id.end()));
	});
	window->on_confirm_remove([this](int mode) { confirmRemove(mode); });
	window->on_cancel_remove([this] { cancelRemove(); });
	window->on_torrents_tab([this] { setActiveTab(0); });
	window->on_search_tab([this] { setActiveTab(1); });
	window->on_preferences_tab([this] { setActiveTab(2); });
	window->on_logs_tab([this] { setActiveTab(3); });
	window->on_select_category([this](int filter) { setCategoryFilter(filter); });
	window->on_filter_torrents([this](const slint::SharedString &filter) {
		setTextFilter(std::string(filter.begin(), filter.end()));
	});
	window->on_sort_torrents([this](int field) { sortTorrents(field); });
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
		addSelectedSearchResult(std::string(id.begin(), id.end()));
	});
	window->on_select_details_tab([this](int tab) { setSelectedDetailsTab(tab); });
	window->on_details_action([this](int action) { detailsAction(action); });
	window->on_preview_detail_file([this](int index) { previewDetailFile(index); });
	window->on_set_file_priority([this](int index, int priority) { setFilePriority(index, priority); });
	window->on_set_speed_limits([this](const slint::SharedString &download, const slint::SharedString &upload) {
		setSpeedLimits(std::string(download.begin(), download.end()), std::string(upload.begin(), upload.end()));
	});
	window->on_set_sequential([this](bool enabled) { setSequentialDownload(enabled); });
	window->on_clear_logs([this] { clearLogs(); });
	window->on_change_theme([this](int theme) { changeTheme(theme); });
	window->on_toggle_sidebar([this] { toggleSidebar(); });
	window->on_browse_preference_directory([this] { browsePreferenceDirectory(); });
	window->on_apply_preferences([this] { applyPreferences(); });
	window->on_resize_layout([this](int sidebarWidth, int bottomPanelHeight) {
		resizeLayout(sidebarWidth, bottomPanelHeight);
	});
	window->on_add_torrent([this] { openAddDialog(); });
	window->on_submit_add_magnet([this](const slint::SharedString &magnet, const slint::SharedString &savePath) {
		submitMagnet(std::string(magnet.begin(), magnet.end()), std::string(savePath.begin(), savePath.end()));
	});
	window->on_submit_add_file([this](const slint::SharedString &path, const slint::SharedString &savePath) {
		submitTorrentFile(std::string(path.begin(), path.end()), std::string(savePath.begin(), savePath.end()));
	});
	window->on_cancel_add([this] { cancelAdd(); });
	window->on_browse_torrent_file([this] { browseTorrentFile(); });
	window->on_browse_save_directory([this] { browseSaveDirectory(); });
	window->on_navigate_torrent([this](int direction) { navigateTorrent(direction); });
	window->on_focus_search([this] { focusSearch(); });
	window->on_cancel_dialogs([this] {
		cancelAdd();
		cancelRemove();
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
	window->set_active_tab(preferences.ui.selectedMainTab);
	window->set_category_filter(torrentPresenter.categoryFilter());
	window->set_torrent_rows(modelAdapter.model());
	window->set_search_rows(searchModelAdapter.model());
	window->set_detail_files(detailsModelAdapter.filesModel());
	window->set_detail_peers(detailsModelAdapter.peersModel());
	window->set_detail_trackers(detailsModelAdapter.trackersModel());
	window->set_log_rows(logModelAdapter.model());
	window->set_logs_state_message(slint::SharedString("Diagnostics are updated from the bounded log buffer."));
	const auto currentPreferences = preferencesController.current();
	selectedDetailsTab_ = std::clamp(currentPreferences.ui.selectedDetailsTab, 0, 4);
	window->set_selected_theme(currentPreferences.theme);
	window->set_preferences_state_message(slint::SharedString("Changes are saved transactionally."));
	window->set_preference_download_limit(slint::SharedString(std::to_string(currentPreferences.downloadSpeedLimit)));
	window->set_preference_upload_limit(slint::SharedString(std::to_string(currentPreferences.uploadSpeedLimit)));
	window->set_preference_download_path(slint::SharedString(currentPreferences.downloadPath));
	window->set_preference_enable_dht(currentPreferences.enableDht);
	window->set_preference_enable_upnp(currentPreferences.enableUpnp);
	window->set_preference_enable_natpmp(currentPreferences.enableNatPmp);
	window->set_preference_torznab_enabled(currentPreferences.torznabEnabled);
	window->set_preference_torznab_url(slint::SharedString(currentPreferences.torznabUrl));
	window->set_preference_proxy_enabled(currentPreferences.proxyEnabled);
	window->set_preference_proxy_type(slint::SharedString(currentPreferences.proxyType));
	window->set_preference_proxy_host(slint::SharedString(currentPreferences.proxyHost));
	window->set_preference_proxy_port(slint::SharedString(std::to_string(currentPreferences.proxyPort)));
	window->set_preference_proxy_username(slint::SharedString(currentPreferences.proxyUsername));
	window->set_selected_details_tab(selectedDetailsTab_);
	window->set_add_dialog_open(false);
	window->set_remove_dialog_open(false);
	window->set_search_query(slint::SharedString());
	refreshSearch();
	refresh();
	refreshTimer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(250), [this] { refresh(); });
	started = true;
}

void SlintAppController::stop()
{
	refreshTimer.stop();
	started = false;
}

void SlintAppController::refresh()
{
	refreshSearch();
	if (const auto saveResult = preferencesController.pollSave())
	{
		window->set_preferences_state_message(slint::SharedString(saveResult->success
			? "Preferences saved" : saveResult->message));
	}
	logsPresenter.update();
	logModelAdapter.update(logsPresenter.buildRows());
	window->set_log_rows(logModelAdapter.model());
	const auto rows = torrentPresenter.buildRows();
	modelAdapter.update(rows);
	window->set_torrent_rows(modelAdapter.model());
	window->set_startup_state(slint::SharedString("Ready"));

	if (torrentPresenter.selectedId().empty())
	{
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

	for (const auto &row : rows)
	{
		if (row.id == torrentPresenter.selectedId())
		{
			const auto hash = torrentPresenter.hashForId(row.id);
			detailsPresenter.setSelectedTorrent(hash);
			detailsPresenter.setSelectedTab(static_cast<Presentation::DetailsTab>(selectedDetailsTab_));
			const auto details = detailsPresenter.buildGeneral();
			window->set_selected_torrent_id(slint::SharedString(row.id));
			window->set_selected_torrent_name(slint::SharedString(row.name));
			window->set_selected_torrent_paused(row.paused);
			if (details)
			{
				window->set_selected_torrent_state(slint::SharedString(details->stateLabel));
				window->set_selected_torrent_progress(slint::SharedString(details->progressLabel));
				window->set_selected_torrent_size(slint::SharedString(details->sizeLabel));
				window->set_selected_torrent_down_rate(slint::SharedString(details->downloadRateLabel));
				window->set_selected_torrent_up_rate(slint::SharedString(details->uploadRateLabel));
				window->set_selected_torrent_eta(slint::SharedString(details->etaLabel));
				window->set_selected_torrent_save_path(slint::SharedString(details->savePath));
				if (const auto settings = detailsPresenter.buildSettings())
				{
					window->set_selected_sequential_download(settings->sequentialDownload);
					window->set_selected_download_limit(slint::SharedString(std::to_string(settings->downloadLimitBytes)));
					window->set_selected_upload_limit(slint::SharedString(std::to_string(settings->uploadLimitBytes)));
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
					if (selectedDetailsTab_ == 1)
						detailsModelAdapter.updateFiles(section.files);
					else if (selectedDetailsTab_ == 2)
						detailsModelAdapter.updatePeers(section.peers);
					else
						detailsModelAdapter.updateTrackers(section.trackers);
					if (!section.message.empty())
						window->set_details_message(slint::SharedString(section.message));
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
}

void SlintAppController::refreshSearch()
{
	searchPresenter.update();
	searchModelAdapter.update(searchPresenter.buildResultRows());
	window->set_search_rows(searchModelAdapter.model());
	window->set_search_loading(searchPresenter.isSearching());
	window->set_search_can_load_more(searchPresenter.hasMore() && !searchPresenter.query().empty());
	std::string message = searchPresenter.stateMessage();
	if (message.empty() && searchPresenter.isSearching())
		message = "Searching...";
	window->set_search_state_message(slint::SharedString(message));
}

void SlintAppController::selectTorrent(const std::string &id)
{
	torrentPresenter.setSelectedId(id);
	refresh();
}

void SlintAppController::executeCommand(const std::string &id, int command)
{
	TorrentCommand torrentCommand = TorrentCommand::Pause;
	switch (command)
	{
	case 0:
		torrentCommand = TorrentCommand::Pause;
		break;
	case 1:
		torrentCommand = TorrentCommand::Resume;
		break;
	case 2:
		torrentCommand = TorrentCommand::ForceStart;
		break;
	case 3:
		torrentCommand = TorrentCommand::ForceRecheck;
		break;
	case 4:
		torrentCommand = TorrentCommand::MoveQueueUp;
		break;
	case 5:
		torrentCommand = TorrentCommand::MoveQueueDown;
		break;
	case 6:
		torrentCommand = TorrentCommand::ForceReannounce;
		break;
	default:
		window->set_startup_state(slint::SharedString("Unsupported torrent command"));
		return;
	}

	const auto result = torrentPresenter.executeCommand(id, torrentCommand);
	if (!result.success)
		window->set_startup_state(slint::SharedString(result.message));
	refresh();
}

void SlintAppController::removeTorrent(const std::string &id)
{
	const auto hash = torrentPresenter.hashForId(id);
	if (!hash)
	{
		window->set_startup_state(slint::SharedString("Torrent is no longer available"));
		return;
	}
	pendingRemoveId_ = id;
	std::string name = id;
	for (const auto &row : torrentPresenter.buildRows())
	{
		if (row.id == id)
		{
			name = row.name;
			break;
		}
	}
	window->set_remove_dialog_name(slint::SharedString(name));
	window->set_remove_dialog_open(true);
}

void SlintAppController::confirmRemove(int mode)
{
	if (pendingRemoveId_.empty())
		return;
	const auto removalMode = mode == 1 ? TorrentRemovalMode::DeleteData
		: mode == 2 ? TorrentRemovalMode::DeleteSourceTorrent
		: mode == 3 ? TorrentRemovalMode::DeleteDataAndSourceTorrent
		: TorrentRemovalMode::KeepAllFiles;
	const auto result = torrentPresenter.removeTorrent(pendingRemoveId_, removalMode);
	if (!result.success)
		window->set_startup_state(slint::SharedString(result.message));
	else
	{
		window->set_startup_state(slint::SharedString("Torrent removed"));
		if (torrentPresenter.selectedId() == pendingRemoveId_)
			torrentPresenter.setSelectedId({});
	}
	pendingRemoveId_.clear();
	window->set_remove_dialog_open(false);
	refresh();
}

void SlintAppController::cancelRemove()
{
	pendingRemoveId_.clear();
	window->set_remove_dialog_open(false);
}

void SlintAppController::openAddDialog()
{
	addController.cancel();
	const auto preferences = preferencesController.current();
	const std::string defaultPath = preferences.downloadPath.empty() ? "./downloads" : preferences.downloadPath;
	window->set_add_magnet_uri(slint::SharedString());
	window->set_add_file_path(slint::SharedString());
	window->set_add_save_path(slint::SharedString(defaultPath));
	window->set_add_dialog_message(slint::SharedString("Enter a magnet URI or a .torrent path."));
	window->set_add_dialog_open(true);
}

void SlintAppController::addSelectedSearchResult(const std::string &id)
{
	for (const auto &result : searchPresenter.results())
	{
		const std::string resultId = result.infoHash.empty() ? result.magnetUri : result.infoHash;
		if (resultId != id)
			continue;
		searchPresenter.selectResult(result);
		addController.beginSearchResult(result);
		const auto request = addController.take();
		if (!request)
			return;
		window->set_add_magnet_uri(slint::SharedString(request->value));
		window->set_add_file_path(slint::SharedString());
		const auto preferences = preferencesController.current();
		window->set_add_save_path(slint::SharedString(preferences.downloadPath.empty() ? "./downloads" : preferences.downloadPath));
		window->set_add_dialog_message(slint::SharedString("Search result selected. Choose a destination and add it."));
		window->set_add_dialog_open(true);
		return;
	}
	window->set_search_state_message(slint::SharedString("Search result is no longer available"));
}

void SlintAppController::submitMagnet(const std::string &magnet, const std::string &savePath)
{
	if (magnet.empty())
	{
		window->set_add_dialog_message(slint::SharedString("A magnet URI is required"));
		return;
	}
	addController.beginMagnet(magnet);
	const auto request = addController.take();
	if (!request)
		return;
	const Result result = app.torrentManager().addMagnetTorrent(request->value, savePath.empty() ? "./downloads" : savePath);
	if (!result)
	{
		window->set_add_dialog_message(slint::SharedString(result.message));
		return;
	}
	cancelAdd();
	window->set_startup_state(slint::SharedString("Magnet added"));
	refresh();
}

void SlintAppController::submitTorrentFile(const std::string &path, const std::string &savePath)
{
	if (path.empty())
	{
		window->set_add_dialog_message(slint::SharedString("A .torrent file path is required"));
		return;
	}
	addController.beginFile(path);
	const auto request = addController.take();
	if (!request)
		return;
	const Result result = app.torrentManager().addTorrent(request->value, savePath.empty() ? "./downloads" : savePath);
	if (!result)
	{
		window->set_add_dialog_message(slint::SharedString(result.message));
		return;
	}
	cancelAdd();
	window->set_startup_state(slint::SharedString("Torrent file added"));
	refresh();
}

void SlintAppController::cancelAdd()
{
	addController.cancel();
	window->set_add_dialog_open(false);
}

void SlintAppController::browseTorrentFile()
{
	if (dialogService)
	{
		if (const auto path = dialogService->openTorrentFile())
		{
			window->set_add_file_path(slint::SharedString(path->string()));
			window->set_add_dialog_message(slint::SharedString("Torrent file selected."));
			return;
		}
	}
	window->set_add_dialog_message(slint::SharedString(
		"The native file picker is unavailable or was cancelled; enter the path manually."));
}

void SlintAppController::browseSaveDirectory()
{
	if (dialogService)
	{
		if (const auto path = dialogService->selectDirectory())
		{
			window->set_add_save_path(slint::SharedString(path->string()));
			window->set_add_dialog_message(slint::SharedString("Save directory selected."));
			return;
		}
	}
	window->set_add_dialog_message(slint::SharedString(
		"The native directory picker is unavailable or was cancelled; enter the path manually."));
}

void SlintAppController::setActiveTab(int tab)
{
	tab = std::clamp(tab, 0, 3);
	window->set_active_tab(tab);
	auto preferences = preferencesController.current();
	preferences.ui.selectedMainTab = tab;
	const Result save = preferencesController.beginSave(preferences);
	if (!save)
		window->set_preferences_state_message(slint::SharedString(save.message));
	if (tab == 0)
		refresh();
}

void SlintAppController::setCategoryFilter(int filter)
{
	torrentPresenter.setCategoryFilter(filter);
	window->set_category_filter(torrentPresenter.categoryFilter());
	refresh();
}

void SlintAppController::setTextFilter(const std::string &filter)
{
	torrentPresenter.setTextFilter(filter);
	refresh();
}

void SlintAppController::sortTorrents(int field)
{
	Presentation::TorrentSortField selected = Presentation::TorrentSortField::Queue;
	switch (field)
	{
	case 1: selected = Presentation::TorrentSortField::Name; break;
	case 2: selected = Presentation::TorrentSortField::Size; break;
	case 3: selected = Presentation::TorrentSortField::Progress; break;
	case 4: selected = Presentation::TorrentSortField::Status; break;
	case 6: selected = Presentation::TorrentSortField::DownloadRate; break;
	case 7: selected = Presentation::TorrentSortField::UploadRate; break;
	case 8: selected = Presentation::TorrentSortField::Seeds; break;
	case 9: selected = Presentation::TorrentSortField::Peers; break;
	case 10: selected = Presentation::TorrentSortField::Eta; break;
	default: return;
	}
	if (selected == sortField_)
		sortAscending_ = !sortAscending_;
	else
	{
		sortField_ = selected;
		sortAscending_ = true;
	}
	torrentPresenter.setSort(sortField_, sortAscending_);
	refresh();
}

void SlintAppController::executeSearch(const std::string &query)
{
	const Result result = searchPresenter.startSearch(query);
	if (!result)
		window->set_search_state_message(slint::SharedString(result.message));
	refreshSearch();
}

void SlintAppController::cancelSearch()
{
	searchPresenter.cancel();
	refreshSearch();
}

void SlintAppController::loadMoreSearch()
{
	const Result result = searchPresenter.loadMore();
	if (!result)
		window->set_search_state_message(slint::SharedString(result.message));
	refreshSearch();
}

void SlintAppController::selectSearchResult(const std::string &id)
{
	for (const auto &result : searchPresenter.results())
	{
		const std::string resultId = result.infoHash.empty() ? result.magnetUri : result.infoHash;
		if (resultId == id)
		{
			searchPresenter.selectResult(result);
			window->set_search_state_message(slint::SharedString("Result selected; choose Add torrent to continue."));
			return;
		}
	}
}

void SlintAppController::toggleSearchFavorite(const std::string &id)
{
	for (const auto &result : searchPresenter.results())
	{
		const std::string resultId = result.infoHash.empty() ? result.magnetUri : result.infoHash;
		if (resultId == id)
		{
			if (searchPresenter.isFavorite(result.infoHash))
				searchPresenter.removeFavorite(result.infoHash);
			else
				searchPresenter.addFavorite(result);
			refreshSearch();
			return;
		}
	}
}

void SlintAppController::setSelectedDetailsTab(int tab)
{
	selectedDetailsTab_ = std::clamp(tab, 0, 4);
	window->set_selected_details_tab(selectedDetailsTab_);
	auto preferences = preferencesController.current();
	preferences.ui.selectedDetailsTab = selectedDetailsTab_;
	const Result save = preferencesController.beginSave(preferences);
	if (!save)
		window->set_preferences_state_message(slint::SharedString(save.message));
	refresh();
}

void SlintAppController::detailsAction(int action)
{
	const Result result = action == 0 ? detailsPresenter.openContainingFolder() : detailsPresenter.previewLargestMediaFile();
	window->set_details_message(slint::SharedString(result.success ? "Action completed" : result.message));
}

void SlintAppController::previewDetailFile(int fileIndex)
{
	const Result result = detailsPresenter.previewFile(fileIndex);
	window->set_details_message(slint::SharedString(result.success ? "Preview requested" : result.message));
}

void SlintAppController::setFilePriority(int fileIndex, int priority)
{
	const Result result = detailsPresenter.setFilePriority(fileIndex, priority);
	window->set_details_message(slint::SharedString(result.success ? "File priority updated" : result.message));
	refresh();
}

void SlintAppController::setSpeedLimits(const std::string &downloadLimit, const std::string &uploadLimit)
{
	int download = 0;
	int upload = 0;
	Result result = parseSpeedLimit(downloadLimit, download);
	if (result)
		result = parseSpeedLimit(uploadLimit, upload);
	if (result)
		result = detailsPresenter.setSpeedLimits(download, upload);
	window->set_details_message(slint::SharedString(result.success ? "Speed limits updated" : result.message));
	if (result)
		refresh();
}

void SlintAppController::setSequentialDownload(bool enabled)
{
	const Result result = detailsPresenter.setSequentialDownload(enabled);
	window->set_details_message(slint::SharedString(result.success ? "Sequential setting updated" : result.message));
	refresh();
}

void SlintAppController::clearLogs()
{
	logsPresenter.clear();
	logModelAdapter.update({});
	window->set_log_rows(logModelAdapter.model());
	window->set_logs_state_message(slint::SharedString("Diagnostics cleared"));
}

void SlintAppController::changeTheme(int theme)
{
	auto preferences = preferencesController.current();
	preferences.theme = std::clamp(theme, 0, 4);
	const Result result = preferencesController.beginSave(preferences);
	if (result)
		window->set_selected_theme(preferences.theme);
	else
		window->set_preferences_state_message(slint::SharedString(result.message));
}

void SlintAppController::toggleSidebar()
{
	auto preferences = preferencesController.current();
	preferences.ui.sidebarCollapsed = !preferences.ui.sidebarCollapsed;
	const Result result = preferencesController.beginSave(preferences);
	if (result)
		window->set_sidebar_collapsed(preferences.ui.sidebarCollapsed);
	else
		window->set_preferences_state_message(slint::SharedString(result.message));
}

void SlintAppController::browsePreferenceDirectory()
{
	if (dialogService)
	{
		if (const auto path = dialogService->selectDirectory())
		{
			window->set_preference_download_path(slint::SharedString(path->string()));
			window->set_preferences_state_message(slint::SharedString("Default download directory selected."));
			return;
		}
	}
	window->set_preferences_state_message(slint::SharedString(
		"The native directory picker was unavailable or cancelled; enter the path manually."));
}

void SlintAppController::applyPreferences()
{
	const auto stringProperty = [](const slint::SharedString &value)
	{
		return std::string(value.begin(), value.end());
	};

	int downloadLimit = 0;
	int uploadLimit = 0;
	int proxyPort = preferencesController.current().proxyPort;
	Result result = parseSpeedLimit(stringProperty(window->get_preference_download_limit()), downloadLimit);
	if (result)
		result = parseSpeedLimit(stringProperty(window->get_preference_upload_limit()), uploadLimit);
	if (result)
	{
		const std::string proxyPortText = stringProperty(window->get_preference_proxy_port());
		if (!proxyPortText.empty())
			result = parseProxyPort(proxyPortText, proxyPort);
	}
	if (!result)
	{
		window->set_preferences_state_message(slint::SharedString(result.message));
		return;
	}

	auto preferences = preferencesController.current();
	preferences.downloadSpeedLimit = downloadLimit;
	preferences.uploadSpeedLimit = uploadLimit;
	preferences.downloadPath = stringProperty(window->get_preference_download_path());
	preferences.enableDht = window->get_preference_enable_dht();
	preferences.enableUpnp = window->get_preference_enable_upnp();
	preferences.enableNatPmp = window->get_preference_enable_natpmp();
	preferences.torznabEnabled = window->get_preference_torznab_enabled();
	preferences.torznabUrl = stringProperty(window->get_preference_torznab_url());
	preferences.proxyEnabled = window->get_preference_proxy_enabled();
	preferences.proxyType = stringProperty(window->get_preference_proxy_type());
	preferences.proxyHost = stringProperty(window->get_preference_proxy_host());
	preferences.proxyPort = proxyPort;
	preferences.proxyUsername = stringProperty(window->get_preference_proxy_username());
	result = preferencesController.beginSave(preferences);
	window->set_preferences_state_message(slint::SharedString(result
		? "Saving preferences..." : result.message));
}

void SlintAppController::resizeLayout(int sidebarWidth, int bottomPanelHeight)
{
	auto preferences = preferencesController.current();
	preferences.ui.sidebarWidth = std::clamp(sidebarWidth, 120, 600);
	preferences.ui.bottomPanelHeight = std::clamp(bottomPanelHeight, 120, 1000);
	const Result result = preferencesController.beginSave(preferences);
	if (!result)
		window->set_preferences_state_message(slint::SharedString(result.message));
}

void SlintAppController::navigateTorrent(int direction)
{
	if (direction == 0)
		return;
	const auto rows = torrentPresenter.buildRows();
	if (rows.empty())
		return;

	std::size_t current = direction > 0 ? 0 : rows.size() - 1;
	for (std::size_t index = 0; index < rows.size(); ++index)
	{
		if (rows[index].id == torrentPresenter.selectedId())
		{
			const auto offset = direction > 0 ? 1 : -1;
			const auto candidate = static_cast<std::ptrdiff_t>(index) + offset;
			current = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(candidate, 0,
				static_cast<std::ptrdiff_t>(rows.size() - 1)));
			break;
		}
	}
	selectTorrent(rows[current].id);
}

void SlintAppController::focusSearch()
{
	setActiveTab(1);
}

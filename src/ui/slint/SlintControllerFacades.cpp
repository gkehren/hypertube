#include "SlintControllerFacades.hpp"

#include "app/App.hpp"
#include "presentation/TorrentAddController.hpp"
#include "presentation/UiFormatters.hpp"
#include "SlintString.hpp"
#include "utils/TorrentIdentity.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <limits>
#include <utility>

namespace
{
Result parseSpeedLimit(const std::string &value, int &output)
{
	if (value.empty())
		return Result::Failure("A speed limit is required; use 0 for unlimited", ResultCode::InvalidInput);
	long long parsed = 0;
	const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
	if (error != std::errc{} || end != value.data() + value.size() || parsed < 0
		|| parsed > std::numeric_limits<int>::max())
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

Presentation::TorrentSortField sortField(TorrentSort field, bool &valid)
{
	valid = true;
	switch (field)
	{
	case TorrentSort::Name: return Presentation::TorrentSortField::Name;
	case TorrentSort::Size: return Presentation::TorrentSortField::Size;
	case TorrentSort::Progress: return Presentation::TorrentSortField::Progress;
	case TorrentSort::Status: return Presentation::TorrentSortField::Status;
	case TorrentSort::DownloadRate: return Presentation::TorrentSortField::DownloadRate;
	case TorrentSort::UploadRate: return Presentation::TorrentSortField::UploadRate;
	case TorrentSort::Eta: return Presentation::TorrentSortField::Eta;
	case TorrentSort::Seeds: return Presentation::TorrentSortField::Seeds;
	case TorrentSort::Peers: return Presentation::TorrentSortField::Peers;
	case TorrentSort::Queue: return Presentation::TorrentSortField::Queue;
	}
	valid = false;
	return Presentation::TorrentSortField::Queue;
}

::TorrentCommand torrentCommand(UiTorrentCommand command, bool &valid)
{
	valid = true;
	switch (command)
	{
	case UiTorrentCommand::Pause: return ::TorrentCommand::Pause;
	case UiTorrentCommand::Resume: return ::TorrentCommand::Resume;
	case UiTorrentCommand::ForceStart: return ::TorrentCommand::ForceStart;
	case UiTorrentCommand::ForceRecheck: return ::TorrentCommand::ForceRecheck;
	case UiTorrentCommand::QueueUp: return ::TorrentCommand::MoveQueueUp;
	case UiTorrentCommand::QueueDown: return ::TorrentCommand::MoveQueueDown;
	case UiTorrentCommand::ForceReannounce: return ::TorrentCommand::ForceReannounce;
	}
	valid = false;
	return ::TorrentCommand::Pause;
}
} // namespace

namespace SlintUi
{
TorrentUiController::TorrentUiController(Presentation::TorrentListPresenter &presenter, MainWindow &window,
	Presentation::TorrentDetailsPresenter &detailsPresenter, std::function<void()> refresh,
	std::function<void()> resetDetails, Presentation::TorrentSortField &sortField, bool &sortAscending,
	bool &viewDirty, std::string &pendingRemoveId)
	: presenter_(presenter), window_(window), detailsPresenter_(detailsPresenter), refresh_(std::move(refresh)),
	  resetDetails_(std::move(resetDetails)), sortField_(sortField), sortAscending_(sortAscending),
	  viewDirty_(viewDirty), pendingRemoveId_(pendingRemoveId)
{
}

void TorrentUiController::select(const std::string &id)
{
	if (!validateId(id))
		return;
	presenter_.setSelectedId(id);
	const auto availability = presenter_.availabilityForId(id);
	const auto message = Presentation::availabilityMessage(availability);
	if (!message.empty())
		window_.set_details_message(SlintUi::toSharedString(message));
	if (resetDetails_)
		resetDetails_();
	refresh_();
}

void TorrentUiController::executeCommand(const std::string &id, UiTorrentCommand command)
{
	if (!validateId(id))
		return;
	bool valid = false;
	const auto mapped = torrentCommand(command, valid);
	if (!valid)
	{
		window_.set_startup_state(slint::SharedString("Unsupported torrent command"));
		return;
	}
	const auto result = presenter_.executeCommand(id, mapped);
	viewDirty_ = true;
	if (!result)
		window_.set_startup_state(SlintUi::toSharedString(result.message));
	refresh_();
}

void TorrentUiController::remove(const std::string &id)
{
	if (!validateId(id))
		return;
	pendingRemoveId_ = id;
	std::string name = id;
	for (const auto &row : presenter_.buildRows())
	{
		if (row.id == id)
		{
			name = row.name;
			break;
		}
	}
	window_.set_remove_dialog_name(SlintUi::toSharedString(name));
	window_.set_remove_dialog_open(true);
}

void TorrentUiController::confirmRemove(RemovalMode mode)
{
	if (pendingRemoveId_.empty())
		return;
	const auto removalMode = mode == RemovalMode::DeleteData ? TorrentRemovalMode::DeleteData
		: mode == RemovalMode::DeleteSource ? TorrentRemovalMode::DeleteSourceTorrent
		: mode == RemovalMode::DeleteDataAndSource ? TorrentRemovalMode::DeleteDataAndSourceTorrent
		: TorrentRemovalMode::KeepAllFiles;
	const auto result = presenter_.removeTorrent(pendingRemoveId_, removalMode);
	if (!result.success)
		window_.set_startup_state(SlintUi::toSharedString(result.message));
	else
	{
		window_.set_startup_state(slint::SharedString("Torrent removed"));
		if (presenter_.selectedId() == pendingRemoveId_)
			presenter_.setSelectedId({});
	}
	viewDirty_ = true;
	pendingRemoveId_.clear();
	window_.set_remove_dialog_open(false);
	refresh_();
}

void TorrentUiController::cancelRemove()
{
	pendingRemoveId_.clear();
	window_.set_remove_dialog_open(false);
}

void TorrentUiController::navigate(int direction)
{
	if (direction == 0)
		return;
	const auto rows = presenter_.buildRows();
	if (rows.empty())
		return;

	std::size_t current = direction > 0 ? 0 : rows.size() - 1;
	for (std::size_t index = 0; index < rows.size(); ++index)
	{
		if (rows[index].id == presenter_.selectedId())
		{
			const auto offset = direction > 0 ? 1 : -1;
			const auto candidate = static_cast<std::ptrdiff_t>(index) + offset;
			current = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(candidate, 0,
				static_cast<std::ptrdiff_t>(rows.size() - 1)));
			break;
		}
	}
	select(rows[current].id);
}

void TorrentUiController::copyMagnet(const std::string &id)
{
	if (id.empty())
	{
		window_.set_startup_state(slint::SharedString("No torrent is selected"));
		return;
	}
	if (!validateId(id))
		return;
	const auto hash = presenter_.hashForId(id);
	if (!hash)
		return;
	presenter_.setSelectedId(id);
	detailsPresenter_.setSelectedTorrent(hash);
	const Result result = detailsPresenter_.copyMagnetUri();
	window_.set_details_message(SlintUi::toSharedString(result ? "Magnet URI copied" : result.message));
}

bool TorrentUiController::validateId(const std::string &id, bool allowLoading)
{
	const auto availability = presenter_.availabilityForId(id);
	if (availability.state == Presentation::TorrentAvailability::InvalidId)
	{
		Presentation::logInvalidTorrentId(id, presenter_.collectionRevision(), presenter_.registrySize());
		assert(Utils::TorrentIdentity::isValid(id) && "Invalid torrent ID received from Slint");
	}
	const bool accepted = availability.state == Presentation::TorrentAvailability::Available
		|| availability.state == Presentation::TorrentAvailability::MetadataPending
		|| (allowLoading && availability.state == Presentation::TorrentAvailability::LoadingStatus)
		|| availability.state == Presentation::TorrentAvailability::Error;
	if (!accepted)
		window_.set_startup_state(SlintUi::toSharedString(Presentation::availabilityMessage(availability)));
	return accepted;
}

void TorrentUiController::setCategory(TorrentCategory category)
{
	presenter_.setCategoryFilter(static_cast<int>(category));
	window_.set_category_filter(static_cast<TorrentCategory>(presenter_.categoryFilter()));
	viewDirty_ = true;
	refresh_();
}

void TorrentUiController::setTextFilter(const std::string &filter)
{
	presenter_.setTextFilter(filter);
	viewDirty_ = true;
	refresh_();
}

void TorrentUiController::sort(TorrentSort field)
{
	bool valid = false;
	const auto selected = sortField(field, valid);
	if (!valid)
		return;
	if (selected == sortField_)
		sortAscending_ = !sortAscending_;
	else
	{
		sortField_ = selected;
		sortAscending_ = true;
	}
	presenter_.setSort(sortField_, sortAscending_);
	viewDirty_ = true;
	refresh_();
}

SearchUiController::SearchUiController(Presentation::SearchPresenter &presenter, MainWindow &window,
	std::function<void()> refreshSearch)
	: presenter_(presenter), window_(window), refreshSearch_(std::move(refreshSearch))
{
}

void SearchUiController::execute(const std::string &query)
{
	const auto result = presenter_.startSearch(query);
	if (!result)
		window_.set_search_state_message(SlintUi::toSharedString(result.message));
	refreshSearch_();
}

void SearchUiController::cancel()
{
	presenter_.cancel();
	refreshSearch_();
}

void SearchUiController::loadMore()
{
	const auto result = presenter_.loadMore();
	if (!result)
		window_.set_search_state_message(SlintUi::toSharedString(result.message));
	refreshSearch_();
}

bool SearchUiController::select(const std::string &id)
{
	const auto select = [this, &id](const auto &results)
	{
		for (const auto &result : results)
		{
			const std::string resultId = result.infoHash.empty() ? result.magnetUri : result.infoHash;
			if (resultId == id)
			{
				presenter_.selectResult(result);
				return true;
			}
		}
		return false;
	};
	if (!select(presenter_.results()) && !select(presenter_.favorites()))
	{
		window_.set_search_state_message(slint::SharedString("Search result is no longer available"));
		return false;
	}
	return true;
}

void SearchUiController::toggleFavorite(const std::string &id)
{
	const auto toggle = [this, &id](const auto &results)
	{
		for (const auto &result : results)
		{
			const std::string resultId = result.infoHash.empty() ? result.magnetUri : result.infoHash;
			if (resultId != id)
				continue;
			if (presenter_.isFavorite(result.infoHash))
				presenter_.removeFavorite(result.infoHash);
			else
				presenter_.addFavorite(result);
			return true;
		}
		return false;
	};
	toggle(presenter_.results()) || toggle(presenter_.favorites());
	refreshSearch_();
}

void SearchUiController::selectHistory(const std::string &query)
{
	window_.set_search_query(SlintUi::toSharedString(query));
	execute(query);
}

void SearchUiController::clearHistory()
{
	presenter_.clearHistory();
	refreshSearch_();
	window_.set_search_state_message(slint::SharedString("Search history cleared"));
}

void SearchUiController::removeFavorite(const std::string &id)
{
	presenter_.removeFavorite(id);
	refreshSearch_();
}

DetailsUiController::DetailsUiController(Presentation::TorrentDetailsPresenter &presenter, MainWindow &window,
	std::function<void()> refresh, int &selectedTab, std::function<void()> resetRefresh,
	std::function<void(int)> persistTab)
	: presenter_(presenter), window_(window), refresh_(std::move(refresh)), selectedTab_(selectedTab),
	  resetRefresh_(std::move(resetRefresh)), persistTab_(std::move(persistTab))
{
}

void DetailsUiController::setTab(DetailsTab tab)
{
	selectedTab_ = std::clamp(static_cast<int>(tab), 0, 4);
	window_.set_selected_details_tab(static_cast<DetailsTab>(selectedTab_));
	if (persistTab_)
		persistTab_(selectedTab_);
	resetRefresh_();
	refresh_();
}

void DetailsUiController::action(DetailsAction action)
{
	const auto result = action == DetailsAction::OpenContainingFolder
		? presenter_.openContainingFolder() : presenter_.previewLargestMediaFile();
	window_.set_details_message(SlintUi::toSharedString(result ? "Action requested" : result.message));
}

void DetailsUiController::previewFile(int fileIndex)
{
	const auto result = presenter_.previewFile(fileIndex);
	window_.set_details_message(SlintUi::toSharedString(result ? "Action requested" : result.message));
}

void DetailsUiController::setFilePriority(int fileIndex, int priority)
{
	const auto result = presenter_.setFilePriority(fileIndex, priority);
	window_.set_details_message(SlintUi::toSharedString(result ? "File priority updated" : result.message));
	refresh_();
}

void DetailsUiController::setSpeedLimits(const std::string &downloadLimit, const std::string &uploadLimit)
{
	int download = 0;
	int upload = 0;
	Result result = parseSpeedLimit(downloadLimit, download);
	if (result)
		result = parseSpeedLimit(uploadLimit, upload);
	if (result)
		result = presenter_.setSpeedLimits(download, upload);
	window_.set_details_message(SlintUi::toSharedString(result ? "Speed limits updated" : result.message));
	if (result)
		refresh_();
}

void DetailsUiController::setSequential(bool enabled)
{
	const auto result = presenter_.setSequentialDownload(enabled);
	window_.set_details_message(SlintUi::toSharedString(result ? "Sequential setting updated" : result.message));
	refresh_();
}

PreferencesUiController::PreferencesUiController(Presentation::PreferencesController &preferences,
	Presentation::UiStateController &uiState, MainWindow &window,
	std::function<Presentation::UiStateSnapshot()> currentState)
	: preferences_(preferences), uiState_(uiState), window_(window), currentState_(std::move(currentState))
{
}

void PreferencesUiController::changeTheme(Theme theme)
{
	auto state = currentState_();
	state.theme = std::clamp(static_cast<int>(theme), 0, 4);
	window_.set_selected_theme(static_cast<Theme>(state.theme));
	uiState_.request(state);
}

void PreferencesUiController::toggleSidebar()
{
	auto state = currentState_();
	state.layout.sidebarCollapsed = !state.layout.sidebarCollapsed;
	window_.set_sidebar_collapsed(state.layout.sidebarCollapsed);
	uiState_.request(state);
}

void PreferencesUiController::resizeLayout(int sidebarWidth, int bottomPanelHeight)
{
	auto state = currentState_();
	state.layout.sidebarWidth = std::clamp(sidebarWidth, 120, 600);
	state.layout.bottomPanelHeight = std::clamp(bottomPanelHeight, 120, 1000);
	window_.set_sidebar_width(state.layout.sidebarWidth);
	window_.set_bottom_panel_height(state.layout.bottomPanelHeight);
	uiState_.request(state);
}

void PreferencesUiController::apply()
{
	const auto stringProperty = [](const slint::SharedString &value)
	{
		return std::string(value.begin(), value.end());
	};

	int downloadLimit = 0;
	int uploadLimit = 0;
	int proxyPort = preferences_.current().proxyPort;
	Result result = parseSpeedLimit(stringProperty(window_.get_preference_download_limit()), downloadLimit);
	if (result)
		result = parseSpeedLimit(stringProperty(window_.get_preference_upload_limit()), uploadLimit);
	if (result)
	{
		const std::string proxyPortText = stringProperty(window_.get_preference_proxy_port());
		if (!proxyPortText.empty())
			result = parseProxyPort(proxyPortText, proxyPort);
	}
	if (!result)
	{
		window_.set_preferences_state_message(SlintUi::toSharedString(result.message));
		return;
	}

	auto preferences = preferences_.current();
	preferences.downloadSpeedLimit = downloadLimit;
	preferences.uploadSpeedLimit = uploadLimit;
	preferences.downloadPath = stringProperty(window_.get_preference_download_path());
	preferences.enableDht = window_.get_preference_enable_dht();
	preferences.enableUpnp = window_.get_preference_enable_upnp();
	preferences.enableNatPmp = window_.get_preference_enable_natpmp();
	preferences.torznabEnabled = window_.get_preference_torznab_enabled();
	preferences.torznabUrl = stringProperty(window_.get_preference_torznab_url());
	preferences.proxyEnabled = window_.get_preference_proxy_enabled();
	preferences.proxyType = stringProperty(window_.get_preference_proxy_type());
	preferences.proxyHost = stringProperty(window_.get_preference_proxy_host());
	preferences.proxyPort = proxyPort;
	preferences.proxyUsername = stringProperty(window_.get_preference_proxy_username());
	const auto torznabSecret = window_.get_preference_clear_torznab_secret()
		? std::optional<std::string>("") : std::nullopt;
	const auto proxySecret = window_.get_preference_clear_proxy_secret()
		? std::optional<std::string>("") : std::nullopt;
	const std::string torznabInput = stringProperty(window_.get_preference_torznab_secret());
	const std::string proxyInput = stringProperty(window_.get_preference_proxy_secret());
	const std::optional<std::string> torznabValue = torznabSecret ? torznabSecret
		: (torznabInput.empty() ? std::nullopt : std::optional<std::string>(torznabInput));
	const std::optional<std::string> proxyValue = proxySecret ? proxySecret
		: (proxyInput.empty() ? std::nullopt : std::optional<std::string>(proxyInput));
	result = preferences_.beginSave(preferences, torznabValue, proxyValue);
	window_.set_preferences_state_message(SlintUi::toSharedString(result
		? "Saving preferences..." : result.message));
	if (result)
	{
		window_.set_preference_clear_torznab_secret(false);
		window_.set_preference_clear_proxy_secret(false);
		window_.set_preference_torznab_secret(slint::SharedString());
		window_.set_preference_proxy_secret(slint::SharedString());
	}
}

void PreferencesUiController::clearTorznabSecret()
{
	window_.set_preference_torznab_secret(slint::SharedString());
	window_.set_preference_clear_torznab_secret(true);
	window_.set_preferences_state_message(slint::SharedString("Torznab secret will be removed when applied."));
}

void PreferencesUiController::clearProxySecret()
{
	window_.set_preference_proxy_secret(slint::SharedString());
	window_.set_preference_clear_proxy_secret(true);
	window_.set_preferences_state_message(slint::SharedString("Proxy secret will be removed when applied."));
}

DialogCoordinator::DialogCoordinator(App &app, TorrentAddController &addController,
	Presentation::PreferencesController &preferences, Presentation::SearchPresenter &searchPresenter,
	DialogService &dialogs, MainWindow &window, std::function<void()> refresh)
	: app_(app), addController_(addController), preferences_(preferences), searchPresenter_(searchPresenter),
	 dialogs_(dialogs), window_(window), refresh_(std::move(refresh))
{
}

void DialogCoordinator::openAddDialog()
{
	addController_.cancel();
	const auto preferences = preferences_.current();
	const std::string defaultPath = preferences.downloadPath.empty() ? "./downloads" : preferences.downloadPath;
	window_.set_add_magnet_uri(slint::SharedString());
	window_.set_add_file_path(slint::SharedString());
	window_.set_add_save_path(SlintUi::toSharedString(defaultPath));
	window_.set_add_dialog_message(slint::SharedString("Enter a magnet URI or a .torrent path."));
	window_.set_add_dialog_open(true);
}

void DialogCoordinator::addSelectedSearchResult(const std::string &id)
{
	auto addFrom = [this, &id](const auto &results)
	{
		for (const auto &result : results)
		{
			const std::string resultId = result.infoHash.empty() ? result.magnetUri : result.infoHash;
			if (resultId != id)
				continue;
			searchPresenter_.selectResult(result);
			addController_.beginSearchResult(result);
			const auto request = addController_.take();
			if (!request)
				return true;
			window_.set_add_magnet_uri(SlintUi::toSharedString(request->value));
			window_.set_add_file_path(slint::SharedString());
			const auto preferences = preferences_.current();
			window_.set_add_save_path(SlintUi::toSharedString(
				preferences.downloadPath.empty() ? "./downloads" : preferences.downloadPath));
			window_.set_add_dialog_message(slint::SharedString("Search result selected. Choose a destination and add it."));
			window_.set_add_dialog_open(true);
			return true;
		}
		return false;
	};
	if (!addFrom(searchPresenter_.results()) && !addFrom(searchPresenter_.favorites()))
		window_.set_search_state_message(slint::SharedString("Search result is no longer available"));
}

void DialogCoordinator::submitMagnet(const std::string &magnet, const std::string &savePath)
{
	if (magnet.empty())
	{
		window_.set_add_dialog_message(slint::SharedString("A magnet URI is required"));
		return;
	}
	addController_.beginMagnet(magnet);
	const auto request = addController_.take();
	if (!request)
		return;
	const Result result = app_.torrentManager().addMagnetTorrent(request->value,
		savePath.empty() ? "./downloads" : savePath);
	if (!result)
	{
		window_.set_add_dialog_message(SlintUi::toSharedString(result.message));
		return;
	}
	cancelAdd();
	window_.set_startup_state(slint::SharedString("Magnet added"));
	refresh_();
}

void DialogCoordinator::submitTorrentFile(const std::string &path, const std::string &savePath)
{
	if (path.empty())
	{
		window_.set_add_dialog_message(slint::SharedString("A .torrent file path is required"));
		return;
	}
	addController_.beginFile(path);
	const auto request = addController_.take();
	if (!request)
		return;
	const Result result = app_.torrentManager().addTorrent(request->value,
		savePath.empty() ? "./downloads" : savePath);
	if (!result)
	{
		window_.set_add_dialog_message(SlintUi::toSharedString(result.message));
		return;
	}
	cancelAdd();
	window_.set_startup_state(slint::SharedString("Torrent file added"));
	refresh_();
}

void DialogCoordinator::cancelAdd()
{
	addController_.cancel();
	window_.set_add_dialog_open(false);
}

void DialogCoordinator::browseTorrentFile()
{
	const auto selection = dialogs_.openTorrentFile();
	if (selection.status == DialogSelectionStatus::Selected && selection.path)
	{
		window_.set_add_file_path(SlintUi::toSharedString(selection.path->string()));
		window_.set_add_dialog_message(slint::SharedString("Torrent file selected."));
	}
	else if (selection.status != DialogSelectionStatus::Cancelled && !selection.message.empty())
		window_.set_add_dialog_message(SlintUi::toSharedString(selection.message));
}

void DialogCoordinator::browseSaveDirectory()
{
	const auto selection = dialogs_.selectDirectory();
	if (selection.status == DialogSelectionStatus::Selected && selection.path)
	{
		window_.set_add_save_path(SlintUi::toSharedString(selection.path->string()));
		window_.set_add_dialog_message(slint::SharedString("Save directory selected."));
	}
	else if (selection.status != DialogSelectionStatus::Cancelled && !selection.message.empty())
		window_.set_add_dialog_message(SlintUi::toSharedString(selection.message));
}

void DialogCoordinator::browsePreferenceDirectory()
{
	const auto selection = dialogs_.selectDirectory();
	if (selection.status == DialogSelectionStatus::Selected && selection.path)
	{
		window_.set_preference_download_path(SlintUi::toSharedString(selection.path->string()));
		window_.set_preferences_state_message(slint::SharedString("Default download directory selected."));
	}
	else if (selection.status != DialogSelectionStatus::Cancelled && !selection.message.empty())
		window_.set_preferences_state_message(SlintUi::toSharedString(selection.message));
}

AppShellController::AppShellController(Presentation::LogsPresenter &logs, LogModelAdapter &logModel, MainWindow &window,
	Presentation::UiStateController &uiState, std::function<Presentation::UiStateSnapshot()> currentState,
	bool &viewDirty, std::function<void()> resetDetails, std::function<void()> refresh, bool &focusRequest)
	: logs_(logs), logModel_(logModel), window_(window), uiState_(uiState), currentState_(std::move(currentState)),
	  viewDirty_(viewDirty), resetDetails_(std::move(resetDetails)), refresh_(std::move(refresh)), focusRequest_(focusRequest)
{
}

void AppShellController::setActiveTab(AppTab tab)
{
	const int tabIndex = std::clamp(static_cast<int>(tab), 0, 4);
	tab = static_cast<AppTab>(tabIndex);
	window_.set_active_tab(tab);
	auto state = currentState_();
	state.layout.selectedMainTab = tabIndex;
	uiState_.request(state);
	viewDirty_ = true;
	if (resetDetails_)
		resetDetails_();
	if (tab == AppTab::Torrents)
		refresh_();
}

void AppShellController::clearLogs()
{
	logs_.clear();
	logModel_.update({});
	window_.set_log_rows(logModel_.model());
	window_.set_logs_state_message(slint::SharedString("Diagnostics cleared"));
}

void AppShellController::setLogFilter(LogLevel level, bool enabled)
{
	logs_.setLevelEnabled(static_cast<Utils::LogLevel>(static_cast<int>(level)), enabled);
	window_.set_log_show_debug(logs_.levelEnabled(Utils::LogLevel::Debug));
	window_.set_log_show_info(logs_.levelEnabled(Utils::LogLevel::Info));
	window_.set_log_show_warning(logs_.levelEnabled(Utils::LogLevel::Warning));
	window_.set_log_show_error(logs_.levelEnabled(Utils::LogLevel::Error));
	logModel_.update(logs_.buildRows());
	window_.set_log_rows(logModel_.model());
}

void AppShellController::setLogAutoscroll(bool enabled)
{
	window_.set_log_auto_scroll(enabled);
}

void AppShellController::focusSearch()
{
	setActiveTab(AppTab::Search);
	focusRequest_ = !focusRequest_;
	window_.set_search_focus_request(focusRequest_);
}

void AppShellController::showAbout()
{
	window_.set_startup_state(slint::SharedString("Hypertube - Slint frontend"));
}
} // namespace SlintUi

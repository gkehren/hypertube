#include "SlintRefreshCoordinators.hpp"

#include "Logger.hpp"
#include "SlintString.hpp"

#include <algorithm>

namespace SlintUi
{
TorrentRefreshCoordinator::TorrentRefreshCoordinator(TorrentManager &manager,
	Presentation::TorrentListPresenter &presenter, SlintModelAdapter &model,
	std::shared_ptr<slint::VectorModel<CategoryRow>> categories, MainWindow &window,
	bool &viewDirty, std::vector<Presentation::TorrentRowDto> &visibleRows)
	: manager_(manager), presenter_(presenter), model_(model), categories_(std::move(categories)),
	  window_(window), viewDirty_(viewDirty), visibleRows_(visibleRows)
{
}

void TorrentRefreshCoordinator::refresh(AppTab activeTab)
{
	manager_.requestStatusRefresh();
	const auto revision = manager_.getStatusRevision();
	if (revision != lastStatusRevision_ || viewDirty_)
	{
		while (categories_->row_count() > 0)
			categories_->erase(categories_->row_count() - 1);
		for (const auto &category : presenter_.buildCategories())
			categories_->push_back(CategoryRow{static_cast<TorrentCategory>(category.id),
				SlintUi::toSharedString(category.label), category.count});
		window_.set_categories(categories_);
		if (activeTab == AppTab::Torrents)
		{
			visibleRows_ = presenter_.buildRows();
			model_.update(visibleRows_);
			window_.set_torrent_rows(model_.model());
		}
		viewDirty_ = false;
		lastStatusRevision_ = revision;
	}
	const auto torrents = manager_.getTorrentSnapshot();
	const auto statuses = manager_.getStatusCache();
	const bool waiting = !torrents.empty() && (!statuses || statuses->empty());
	window_.set_startup_state(slint::SharedString(waiting ? "Loading torrent statuses..." : "Ready"));
}

SearchRefreshCoordinator::SearchRefreshCoordinator(Presentation::SearchPresenter &presenter,
	SearchEngine &engine, SearchModelAdapter &results, SearchModelAdapter &favorites,
	std::shared_ptr<slint::VectorModel<slint::SharedString>> recent, MainWindow &window)
	: presenter_(presenter), engine_(engine), results_(results), favorites_(favorites),
	  recent_(std::move(recent)), window_(window)
{
}

void SearchRefreshCoordinator::refreshIfNeeded(AppTab activeTab)
{
	const bool visible = activeTab == AppTab::Search || activeTab == AppTab::Favorites;
	if (!visible && !presenter_.isSearching())
		return;
	presenter_.update();
	if (presenter_.revision() != lastRevision_ || presenter_.isSearching())
		forceRefresh();
}

void SearchRefreshCoordinator::forceRefresh()
{
	results_.update(presenter_.buildResultRows());
	window_.set_search_rows(results_.model());
	favorites_.update(presenter_.buildResultRows(true));
	window_.set_favorite_rows(favorites_.model());
	while (recent_->row_count() > 0)
		recent_->erase(recent_->row_count() - 1);
	const auto history = engine_.getSearchHistory();
	for (std::size_t index = 0; index < history.size() && index < 5; ++index)
		recent_->push_back(SlintUi::toSharedString(history[index]));
	window_.set_recent_searches(recent_);
	window_.set_search_loading(presenter_.isSearching());
	window_.set_search_can_load_more(presenter_.hasMore() && !presenter_.query().empty());
	std::string message = presenter_.stateMessage();
	if (message.empty() && presenter_.isSearching())
		message = "Searching...";
	window_.set_search_state_message(SlintUi::toSharedString(message));
	window_.set_favorites_state_message(SlintUi::toSharedString(presenter_.favorites().empty()
		? "Save search results here for quick access." : "Saved search results"));
	lastRevision_ = presenter_.revision();
}

LogRefreshCoordinator::LogRefreshCoordinator(Presentation::LogsPresenter &presenter,
	LogModelAdapter &model, MainWindow &window)
	: presenter_(presenter), model_(model), window_(window)
{
}

void LogRefreshCoordinator::refresh(AppTab activeTab)
{
	presenter_.update();
	const auto revision = Utils::Logger::revision();
	if (activeTab != AppTab::Logs || revision == lastRevision_)
		return;
	model_.update(presenter_.buildRows());
	window_.set_log_rows(model_.model());
	lastRevision_ = revision;
}

DetailsRefreshCoordinator::DetailsRefreshCoordinator(Presentation::TorrentListPresenter &torrentPresenter,
	Presentation::TorrentDetailsPresenter &detailsPresenter, DetailsModelAdapter &model,
	MainWindow &window, int &selectedTab, std::vector<Presentation::TorrentRowDto> &visibleRows)
	: torrentPresenter_(torrentPresenter), detailsPresenter_(detailsPresenter), model_(model),
	  window_(window), selectedTab_(selectedTab), visibleRows_(visibleRows)
{
}

void DetailsRefreshCoordinator::reset()
{
	lastRefresh_ = {};
	lastRevision_ = 0;
	lastTorrentId_.clear();
	lastTab_ = -1;
}

void DetailsRefreshCoordinator::clearSelection()
{
	detailsPresenter_.setSelectedTorrent(std::nullopt);
	clear("Select a torrent");
}

void DetailsRefreshCoordinator::clearDetailsContentPreservingSelection(
	const std::string &id, const std::string &name, const std::string &message)
{
	window_.set_selected_torrent_id(SlintUi::toSharedTorrentId(id));
	window_.set_selected_torrent_name(SlintUi::toSharedString(name));
	window_.set_selected_torrent_state(slint::SharedString());
	window_.set_selected_torrent_progress(slint::SharedString());
	window_.set_selected_torrent_progress_value(0.0f);
	window_.set_selected_torrent_size(slint::SharedString());
	window_.set_selected_torrent_down_rate(slint::SharedString());
	window_.set_selected_torrent_up_rate(slint::SharedString());
	window_.set_selected_torrent_eta(slint::SharedString());
	window_.set_selected_torrent_save_path(slint::SharedString());
	window_.set_selected_download_limit(slint::SharedString());
	window_.set_selected_upload_limit(slint::SharedString());
	window_.set_selected_torrent_paused(false);
	window_.set_selected_sequential_download(false);
	window_.set_details_message(SlintUi::toSharedString(message));
	model_.updateFiles({});
	model_.updatePeers({});
	model_.updateTrackers({});
}

void DetailsRefreshCoordinator::clear(const std::string &message)
{
	window_.set_selected_torrent_id(slint::SharedString());
	window_.set_selected_torrent_name(slint::SharedString());
	window_.set_selected_torrent_state(slint::SharedString());
	window_.set_selected_torrent_progress(slint::SharedString());
	window_.set_selected_torrent_progress_value(0.0f);
	window_.set_selected_torrent_size(slint::SharedString());
	window_.set_selected_torrent_down_rate(slint::SharedString());
	window_.set_selected_torrent_up_rate(slint::SharedString());
	window_.set_selected_torrent_eta(slint::SharedString());
	window_.set_selected_torrent_save_path(slint::SharedString());
	window_.set_selected_download_limit(slint::SharedString());
	window_.set_selected_upload_limit(slint::SharedString());
	window_.set_selected_torrent_paused(false);
	window_.set_selected_sequential_download(false);
	window_.set_details_message(SlintUi::toSharedString(message));
	model_.updateFiles({});
	model_.updatePeers({});
	model_.updateTrackers({});
	reset();
}

void DetailsRefreshCoordinator::refresh(AppTab activeTab)
{
	if (activeTab != AppTab::Torrents)
		return;
	const auto now = std::chrono::steady_clock::now();
	const auto interval = selectedTab_ == 2 || selectedTab_ == 3
		? std::chrono::seconds(2) : std::chrono::milliseconds(500);
	if (lastRefresh_ != std::chrono::steady_clock::time_point{} && now - lastRefresh_ < interval)
		return;
	lastRefresh_ = now;
	const std::string selectedId = torrentPresenter_.selectedId();
	if (selectedId.empty())
	{
		clearSelection();
		return;
	}

	const Presentation::TorrentRowDto *selectedRow = nullptr;
	for (const auto &row : visibleRows_)
		if (row.id == selectedId) { selectedRow = &row; break; }
	std::optional<Presentation::TorrentRowDto> resolved;
	if (!selectedRow)
	{
		resolved = torrentPresenter_.findRowById(selectedId);
		if (resolved) selectedRow = &*resolved;
	}
	if (!selectedRow)
	{
		const auto availability = torrentPresenter_.availabilityForId(selectedId);
		if (availability.state == Presentation::TorrentAvailability::InvalidId)
			Presentation::logInvalidTorrentId(selectedId, torrentPresenter_.collectionRevision(),
				torrentPresenter_.registrySize());
		clearSelection();
		window_.set_details_message(SlintUi::toSharedString(Presentation::availabilityMessage(availability)));
		return;
	}

	const bool selectionChanged = selectedId != lastTorrentId_;
	if (selectionChanged)
	{
		clearDetailsContentPreservingSelection(selectedRow->id, selectedRow->name, "Loading details...");
	}

	const auto hash = torrentPresenter_.hashForId(selectedRow->id);
	const auto availability = torrentPresenter_.availabilityForId(selectedId);
	detailsPresenter_.setSelectedTorrent(hash);
	detailsPresenter_.setSelectedTab(static_cast<Presentation::DetailsTab>(selectedTab_));
	const auto details = detailsPresenter_.buildGeneral();
	window_.set_selected_torrent_id(SlintUi::toSharedTorrentId(selectedRow->id));
	window_.set_selected_torrent_name(SlintUi::toSharedString(selectedRow->name));
	window_.set_selected_torrent_paused(selectedRow->paused);
	if (!details)
	{
		const auto availability = torrentPresenter_.availabilityForId(selectedId);
		clearDetailsContentPreservingSelection(selectedRow->id, selectedRow->name, Presentation::availabilityMessage(availability));
		return;
	}
	window_.set_selected_torrent_state(SlintUi::toSharedString(details->stateLabel));
	window_.set_selected_torrent_progress(SlintUi::toSharedString(details->progressLabel));
	window_.set_selected_torrent_progress_value(details->progress);
	window_.set_selected_torrent_size(SlintUi::toSharedString(details->sizeLabel));
	window_.set_selected_torrent_down_rate(SlintUi::toSharedString(details->downloadRateLabel));
	window_.set_selected_torrent_up_rate(SlintUi::toSharedString(details->uploadRateLabel));
	window_.set_selected_torrent_eta(SlintUi::toSharedString(details->etaLabel));
	window_.set_selected_torrent_save_path(SlintUi::toSharedString(details->savePath));
	if (const auto settings = detailsPresenter_.buildSettings())
	{
		window_.set_selected_sequential_download(settings->sequentialDownload);
		window_.set_selected_download_limit(SlintUi::toSharedString(std::to_string(settings->downloadLimitBytes)));
		window_.set_selected_upload_limit(SlintUi::toSharedString(std::to_string(settings->uploadLimitBytes)));
	}
	else
	{
		window_.set_selected_sequential_download(false);
		window_.set_selected_download_limit(slint::SharedString());
		window_.set_selected_upload_limit(slint::SharedString());
	}
	const auto availabilityText = Presentation::availabilityMessage(availability);
	window_.set_details_message(availabilityText.empty()
		? slint::SharedString("General") : SlintUi::toSharedString(availabilityText));
	if (selectedTab_ >= 1 && selectedTab_ <= 3)
	{
		const auto section = detailsPresenter_.buildSection(detailsPresenter_.selectedTab());
		const bool changed = selectionChanged || lastTab_ != selectedTab_
			|| lastRevision_ != section.revision;
		if (changed && selectedTab_ == 1) model_.updateFiles(section.files);
		if (changed && selectedTab_ == 2) model_.updatePeers(section.peers);
		if (changed && selectedTab_ == 3) model_.updateTrackers(section.trackers);
		lastRevision_ = section.revision;
		lastTorrentId_ = selectedRow->id;
		lastTab_ = selectedTab_;
		if (!section.message.empty()) window_.set_details_message(SlintUi::toSharedString(section.message));
		else if (section.state == Presentation::DetailsState::Loading) window_.set_details_message(slint::SharedString("Loading details..."));
		else if (section.files.empty() && section.peers.empty() && section.trackers.empty()) window_.set_details_message(slint::SharedString("No details available"));
	}
	else if (selectedTab_ == 4)
	{
		lastTorrentId_ = selectedRow->id;
		lastTab_ = selectedTab_;
		window_.set_details_message(slint::SharedString("Settings ready"));
	}
}

NotificationController::NotificationController(Utils::SystemUtils::SystemOpener &opener,
	Presentation::TorrentListPresenter &presenter, MainWindow &window)
	: opener_(opener), presenter_(presenter), window_(window)
{
}

void NotificationController::drain()
{
	for (const auto &operation : opener_.drainResults())
	{
		message_ = operation.result ? (operation.kind == Utils::SystemUtils::OpenOperationKind::Explorer
			? "Folder opened successfully" : "Preview opened successfully") : operation.result.message;
		targetsDetails_ = !presenter_.selectedId().empty();
		deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	}
	if (message_.empty()) return;
	if (std::chrono::steady_clock::now() >= deadline_) { message_.clear(); return; }
	if (targetsDetails_) window_.set_details_message(SlintUi::toSharedString(message_));
	else window_.set_startup_state(SlintUi::toSharedString(message_));
}
} // namespace SlintUi

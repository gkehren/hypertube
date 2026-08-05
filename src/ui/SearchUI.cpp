#include "SearchUI.hpp"
#include "StringUtils.hpp"
#include "presentation/UiFormatters.hpp"
#include "SystemUtils.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstring>

SearchUI::SearchUI(SearchEngine &searchEngine)
	: searchEngine(searchEngine), isSearching(false)
{
}

void SearchUI::displayIntegratedSearch()
{
	// Search input section with styled header
	HypertubeTheme::drawSectionHeader("Torrent Search");

	// Search input with better styling
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 8.0f));
	ImGui::PushItemWidth(-160.0f); // Leave space for search button
	bool enterPressed = ImGui::InputText("##search", searchQueryBuffer, sizeof(searchQueryBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();
	ImGui::PopStyleVar(2);

	ImGui::SameLine();
	bool searchClicked = HypertubeTheme::drawStyledButton("Search", ImVec2(140, 32), true);

	const auto providers = searchEngine.getSearchProviders();
	if (!providers.empty())
	{
		ImGui::Spacing();
		ImGui::Text("Provider:");
		ImGui::SameLine();
		const std::string activeProvider = searchEngine.getActiveSearchProvider();
		if (ImGui::BeginCombo("##search-provider", activeProvider.c_str()))
		{
			for (const auto &provider : providers)
			{
				const bool selected = provider == activeProvider;
				if (ImGui::Selectable(provider.c_str(), selected))
				{
					Result result = searchEngine.setActiveSearchProvider(provider);
					if (!result && onShowFailurePopup)
						onShowFailurePopup(result.message);
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if (enterPressed || searchClicked)
	{
		performSearch(std::string(searchQueryBuffer));
	}

	// Display search history as clickable suggestions
	const auto &history = searchEngine.getSearchHistory();
	if (!history.empty() && !isSearching)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Recent searches:");
		ImGui::Indent(20.0f);

		// Display up to 5 recent searches
		int displayCount = std::min(5, (int)history.size());
		for (int i = 0; i < displayCount; ++i)
		{
			ImGui::PushID(i);
			std::string buttonLabel = history[i];
			if (ImGui::SmallButton(buttonLabel.c_str()))
			{
				// Copy to search buffer and perform search
				strncpy(searchQueryBuffer, history[i].c_str(), sizeof(searchQueryBuffer) - 1);
				searchQueryBuffer[sizeof(searchQueryBuffer) - 1] = '\0';
				performSearch(history[i]);
			}
			ImGui::SameLine();
			ImGui::PopID();
		}
		ImGui::NewLine();
		ImGui::Unindent(20.0f);
	}

	// Show search status with animated indicator
	if (isSearching)
	{
		ImGui::Spacing();
		float pulse = HypertubeTheme::pulse(3.0f);
		ImVec4 loadingColor = HypertubeTheme::lerpColor(
			HypertubeTheme::getCurrentPalette().textSecondary,
			HypertubeTheme::getCurrentPalette().primary,
			pulse);
		ImGui::PushStyleColor(ImGuiCol_Text, loadingColor);
		ImGui::Text("Searching...");
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (HypertubeTheme::drawStyledButton("Cancel", ImVec2(80, 0), false))
		{
			searchEngine.cancelCurrentSearch();
		}
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, HypertubeTheme::getCurrentPalette().primary);
		ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(200.0f, 4.0f), "");
		ImGui::PopStyleColor();
	}
	else if (state == State::Empty || state == State::Cancelled || state == State::Failed)
	{
		ImGui::Spacing();
		const ImVec4 color = state == State::Failed
			? HypertubeTheme::getCurrentPalette().error
			: HypertubeTheme::getCurrentPalette().textSecondary;
		ImGui::TextColored(color, "%s", stateMessage.c_str());
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Display search results if available
	if (!searchResults.empty() || !currentSearchQuery.empty())
	{
		displayEnhancedSearchResults();
	}
	else if (!isSearching)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().textSecondary);
		ImGui::Text("Enter a search query to find torrents...");
		ImGui::PopStyleColor();
	}
}

void SearchUI::displayEnhancedSearchResults()
{
	// Results header with count
	ImGui::Text("Search Results (%d found for \"%s\"):", (int)searchResults.size(), currentSearchQuery.c_str());

	ImGui::Separator();

	// Create a table for search results with improved styling
	if (ImGui::BeginTable("SearchResultsTable", 10,
						  ImGuiTableFlags_Borders |
							  ImGuiTableFlags_Resizable |
							  ImGuiTableFlags_Sortable |
							  ImGuiTableFlags_ScrollY |
							  ImGuiTableFlags_RowBg |
							  ImGuiTableFlags_ContextMenuInBody,
						  ImVec2(0, -50))) // Leave space for bottom pagination
	{
		// Setup columns with better widths and sorting
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 90);
		ImGui::TableSetupColumn("Seeds", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 60);
		ImGui::TableSetupColumn("Leech", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 60);
		ImGui::TableSetupColumn("Ratio", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 60);
		ImGui::TableSetupColumn("Completed", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 80);
		ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 100);
		ImGui::TableSetupColumn("Last Seen", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 100);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 80);
		ImGui::TableHeadersRow();

		// Handle sorting
		if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs())
		{
			if (resultsChanged)
				sort_specs->SpecsDirty = true;
			if (sort_specs->SpecsDirty)
			{
				sortTorrentResults(searchResults, sort_specs);
				resultsChanged = false;
			}
		}

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(searchResults.size()));
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				displayEnhancedSearchResultRow(searchResults[i], i);
			}
		}

		ImGui::EndTable();
	}

	// Pagination controls at the bottom
	ImGui::PushID("bottom_pagination");
	displayPaginationControls();
	ImGui::PopID();
}

void SearchUI::displayFavorites()
{
	uint64_t currentRevision = searchEngine.getFavoritesRevision();
	bool revisionChanged = (currentRevision != lastFavoritesRevision);

	// Update local cache if favorites have changed
	if (revisionChanged)
	{
		favoritesDisplay = searchEngine.getFavorites();
		lastFavoritesRevision = currentRevision;
	}

	HypertubeTheme::drawSectionHeader("Favorites");

	if (favoritesDisplay.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, HypertubeTheme::getCurrentPalette().textSecondary);
		ImGui::Text("No favorites yet. Right-click on search results to add torrents to favorites.");
		ImGui::PopStyleColor();
		return;
	}

	ImGui::Text("Saved Torrents (%d):", (int)favoritesDisplay.size());
	ImGui::Separator();

	// Create a table for favorites
	if (ImGui::BeginTable("FavoritesTable", 10,
						  ImGuiTableFlags_Borders |
							  ImGuiTableFlags_Resizable |
							  ImGuiTableFlags_Sortable |
							  ImGuiTableFlags_ScrollY |
							  ImGuiTableFlags_RowBg |
							  ImGuiTableFlags_ContextMenuInBody,
						  ImVec2(0, -10)))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90);
		ImGui::TableSetupColumn("Seeds", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Leech", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Ratio", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Completed", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthFixed, 100);
		ImGui::TableSetupColumn("Last Seen", ImGuiTableColumnFlags_WidthFixed, 100);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 80);
		ImGui::TableHeadersRow();

		// Handle sorting
		if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs())
		{
			// Force re-sort if data changed
			if (revisionChanged)
			{
				sort_specs->SpecsDirty = true;
			}

			if (sort_specs->SpecsDirty)
			{
				sortTorrentResults(favoritesDisplay, sort_specs);
			}
		}

		ImGuiListClipper favClipper;
		favClipper.Begin(static_cast<int>(favoritesDisplay.size()));
		while (favClipper.Step())
		{
			for (int i = favClipper.DisplayStart; i < favClipper.DisplayEnd; ++i)
			{
				displayFavoriteRow(favoritesDisplay[i], i);
			}
		}

		ImGui::EndTable();
	}
}

void SearchUI::displayFavoriteRow(const TorrentSearchResult &result, int index)
{
	const float rowHeight = 26.0f;
	ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
	ImGui::PushID(index);

	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Name column
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	std::string displayName = result.name;
	if (displayName.length() > 60)
	{
		displayName = displayName.substr(0, 57) + "...";
	}

	if (ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
	{
		handleSearchResultSelection(result);
	}

	// Context menu for removal
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Remove from Favorites"))
		{
			searchEngine.removeFromFavorites(result.infoHash);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Download"))
		{
			handleSearchResultSelection(result);
		}
		ImGui::EndPopup();
	}

	// Tooltip for full name if truncated
	if (ImGui::IsItemHovered() && result.name.length() > 60)
	{
		ImGui::BeginTooltip();
		ImGui::Text("%s", result.name.c_str());
		ImGui::EndTooltip();
	}

	// Size
	ImGui::TableSetColumnIndex(1);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", Presentation::UiFormatters::formatBytes(static_cast<std::int64_t>(result.sizeBytes)).c_str());

	// Seeders with color coding
	ImGui::TableSetColumnIndex(2);
	ImGui::AlignTextToFramePadding();
	if (result.seeders >= 10)
		ImGui::TextColored(palette.success, "%d", result.seeders);
	else if (result.seeders >= 1)
		ImGui::TextColored(palette.warning, "%d", result.seeders);
	else
		ImGui::TextColored(palette.error, "%d", result.seeders);

	// Leechers
	ImGui::TableSetColumnIndex(3);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d", result.leechers);

	// Seed/Leech ratio
	ImGui::TableSetColumnIndex(4);
	ImGui::AlignTextToFramePadding();
	if (result.leechers > 0)
	{
		float ratio = (float)result.seeders / result.leechers;
		ImVec4 ratioColor = HypertubeTheme::getHealthColor(ratio);
		ImGui::TextColored(ratioColor, "%.1f", ratio);
	}
	else if (result.seeders > 0)
	{
		ImGui::TextColored(palette.success, "∞");
	}
	else
	{
		ImGui::Text("-");
	}

	// Completed count
	ImGui::TableSetColumnIndex(5);
	ImGui::AlignTextToFramePadding();
	if (result.completed > 0)
		ImGui::TextColored(palette.success, "%d", result.completed);
	else
		ImGui::Text("%d", result.completed);

	// Created date
	ImGui::TableSetColumnIndex(6);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", Presentation::UiFormatters::formatUnixDate(result.createdUnix).c_str());

	// Last seen (scraped date)
	ImGui::TableSetColumnIndex(7);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", Presentation::UiFormatters::formatUnixDate(result.scrapedDate).c_str());

	// Category
	ImGui::TableSetColumnIndex(8);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", result.category.c_str());

	// Download button
	ImGui::TableSetColumnIndex(9);
	float buttonHeight = ImGui::GetFrameHeight();
	float buttonVerticalPadding = (rowHeight - buttonHeight) * 0.5f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + buttonVerticalPadding);

	ImGui::PushStyleColor(ImGuiCol_Button, palette.accent);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(palette.accent.x * 1.2f, palette.accent.y * 1.2f, palette.accent.z * 1.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(palette.accent.x * 0.8f, palette.accent.y * 0.8f, palette.accent.z * 0.8f, 1.0f));
	if (ImGui::Button("Download", ImVec2(-1, buttonHeight)))
	{
		handleSearchResultSelection(result);
	}
	ImGui::PopStyleColor(3);

	ImGui::PopID();
}

void SearchUI::displayEnhancedSearchResultRow(const TorrentSearchResult &result, int index)
{
	const float rowHeight = 26.0f;
	ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
	ImGui::PushID(index);

	// Check if this is a favorite (needed for context menu)
	bool isFavorite = isInFavorites(result.infoHash);

	// Name column with truncation for very long names
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	std::string displayName = result.name;
	if (displayName.length() > 60)
	{
		displayName = displayName.substr(0, 57) + "...";
	}

	if (ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
	{
		handleSearchResultSelection(result);
	}

	// Context menu for favorites
	if (ImGui::BeginPopupContextItem())
	{
		if (isFavorite)
		{
			if (ImGui::MenuItem("Remove from Favorites"))
			{
				searchEngine.removeFromFavorites(result.infoHash);
			}
		}
		else
		{
			if (ImGui::MenuItem("Add to Favorites"))
			{
				searchEngine.addToFavorites(result);
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Download"))
		{
			handleSearchResultSelection(result);
		}
		ImGui::EndPopup();
	}

	// Tooltip for full name if truncated
	if (ImGui::IsItemHovered() && result.name.length() > 60)
	{
		ImGui::BeginTooltip();
		ImGui::Text("%s", result.name.c_str());
		ImGui::EndTooltip();
	}

	const auto &palette = HypertubeTheme::getCurrentPalette();

	// Size
	ImGui::TableSetColumnIndex(1);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", Presentation::UiFormatters::formatBytes(static_cast<std::int64_t>(result.sizeBytes)).c_str());

	// Seeders with color coding
	ImGui::TableSetColumnIndex(2);
	ImGui::AlignTextToFramePadding();
	if (result.seeders >= 10)
		ImGui::TextColored(palette.success, "%d", result.seeders);
	else if (result.seeders >= 1)
		ImGui::TextColored(palette.warning, "%d", result.seeders);
	else
		ImGui::TextColored(palette.error, "%d", result.seeders);

	// Leechers
	ImGui::TableSetColumnIndex(3);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d", result.leechers);

	// Seed/Leech ratio
	ImGui::TableSetColumnIndex(4);
	ImGui::AlignTextToFramePadding();
	if (result.leechers > 0)
	{
		float ratio = (float)result.seeders / result.leechers;
		ImVec4 ratioColor = HypertubeTheme::getHealthColor(ratio);
		ImGui::TextColored(ratioColor, "%.1f", ratio);
	}
	else if (result.seeders > 0)
	{
		ImGui::TextColored(palette.success, "∞");
	}
	else
	{
		ImGui::Text("-");
	}

	// Completed count
	ImGui::TableSetColumnIndex(5);
	ImGui::AlignTextToFramePadding();
	if (result.completed > 0)
		ImGui::TextColored(palette.success, "%d", result.completed);
	else
		ImGui::Text("%d", result.completed);

	// Created date
	ImGui::TableSetColumnIndex(6);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", Presentation::UiFormatters::formatUnixDate(result.createdUnix).c_str());

	// Last seen (scraped date)
	ImGui::TableSetColumnIndex(7);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", Presentation::UiFormatters::formatUnixDate(result.scrapedDate).c_str());

	// Category
	ImGui::TableSetColumnIndex(8);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", result.category.c_str());

	// Action button with improved styling - center the button vertically
	ImGui::TableSetColumnIndex(9);
	float buttonHeight = ImGui::GetFrameHeight();
	float buttonVerticalPadding = (rowHeight - buttonHeight) * 0.5f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + buttonVerticalPadding);

	ImGui::PushStyleColor(ImGuiCol_Button, palette.accent);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(palette.accent.x * 1.2f, palette.accent.y * 1.2f, palette.accent.z * 1.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(palette.accent.x * 0.8f, palette.accent.y * 0.8f, palette.accent.z * 0.8f, 1.0f));
	if (ImGui::Button("Download", ImVec2(-1, buttonHeight)))
	{
		handleSearchResultSelection(result);
	}
	ImGui::PopStyleColor(3);

	ImGui::PopID();
}

void SearchUI::handleSearchResultSelection(const TorrentSearchResult &result)
{
	selectedSearchResult = result;
	if (onSearchResultSelected)
	{
		onSearchResultSelected(result);
	}
}

void SearchUI::performSearch(const std::string &query)
{
	if (query.empty() || isSearching.load())
		return;

	SearchQuery searchQuery(query);
	uint64_t requestId = 0;
	Result result = searchEngine.startSearch(searchQuery, requestId);
	if (!result)
	{
		if (onShowFailurePopup)
			onShowFailurePopup(result.message);
		return;
	}
	isSearching = true;
	state = State::Loading;
	stateMessage.clear();
	loadingMore = false;
	activeRequestId = requestId;
	searchResults.clear();
	currentSearchQuery = query;
	nextToken.clear();
	hasMoreResults = true;
}

void SearchUI::displayPaginationControls()
{
	ImGui::Separator();

	ImGui::Text("Results: %d found", (int)searchResults.size());

	// Load more results button
	ImGui::SameLine();
	if (hasMoreResults && !isSearching && !searchResults.empty())
	{
		if (ImGui::Button("Load More"))
		{
			loadMoreResults();
		}
	}
	else
	{
		ImGui::BeginDisabled();
		ImGui::Button("Load More");
		ImGui::EndDisabled();
	}

	if (isSearching)
	{
		ImGui::SameLine();
		ImGui::Text("Loading...");
	}
}

void SearchUI::loadMoreResults()
{
	if (hasMoreResults && !isSearching.load() && !currentSearchQuery.empty() && !nextToken.empty())
	{
		SearchQuery searchQuery(currentSearchQuery, 0, nextToken);
		uint64_t requestId = 0;
		Result result = searchEngine.startSearch(searchQuery, requestId);
		if (!result)
		{
			if (onShowFailurePopup)
				onShowFailurePopup(result.message);
			return;
		}
		isSearching = true;
		state = State::Loading;
		loadingMore = true;
		activeRequestId = requestId;
	}
}

void SearchUI::formatUnixTime(int64_t unixTime, char *buffer, size_t bufferSize)
{
	if (bufferSize == 0)
	{
		return;
	}
	const std::string formatted = Presentation::UiFormatters::formatUnixDate(unixTime);
	std::snprintf(buffer, bufferSize, "%s", formatted.c_str());
}

void SearchUI::setSearchResultSelectedCallback(std::function<void(const TorrentSearchResult &)> callback)
{
	onSearchResultSelected = callback;
}

void SearchUI::setShowFailurePopupCallback(std::function<void(const std::string &)> callback)
{
	onShowFailurePopup = callback;
}

void SearchUI::processPendingResults()
{
	auto completion = searchEngine.takeCompletedSearch();
	if (completion && completion->requestId == activeRequestId)
	{
		isSearching = false;

		if (!completion->result)
		{
			const std::string &errorMsg = completion->result.message;

			// Cancellation is an expected terminal state, not an application error.
			if (completion->result.code == ResultCode::Cancelled)
			{
				state = State::Cancelled;
				stateMessage = "Search cancelled.";
			}
			else
			{
				state = State::Failed;
				stateMessage = "Search failed: " + errorMsg;
				if (onShowFailurePopup)
				{
					onShowFailurePopup(stateMessage);
				}
			}
		}
		else
		{
			// Check if this is a "load more" request by seeing if we already have results
			if (loadingMore)
			{
				mergeUniqueResults(std::move(completion->response.torrents));
			}
			else
			{
				searchResults.clear();
				mergeUniqueResults(std::move(completion->response.torrents));
			}

			nextToken = completion->response.nextToken;
			hasMoreResults = completion->response.hasMore;
			state = searchResults.empty() ? State::Empty : State::Results;
			stateMessage = searchResults.empty() ? "No results found for this query." : std::string{};
		}
		loadingMore = false;
	}
}

void SearchUI::update()
{
	processPendingResults();
}

void SearchUI::mergeUniqueResults(std::vector<TorrentSearchResult> results)
{
	for (auto &result : results)
	{
		const auto duplicate = std::find_if(searchResults.begin(), searchResults.end(), [&result](const TorrentSearchResult &existing)
		{
			return !result.infoHash.empty() && existing.infoHash == result.infoHash;
		});
		if (duplicate == searchResults.end())
			searchResults.push_back(std::move(result));
	}
	resultsChanged = true;
}

bool SearchUI::isInFavorites(const std::string &infoHash) const
{
	return searchEngine.isFavorite(infoHash);
}

void SearchUI::sortTorrentResults(std::vector<TorrentSearchResult> &results, ImGuiTableSortSpecs *sort_specs)
{
	if (!sort_specs || !sort_specs->SpecsDirty || sort_specs->SpecsCount <= 0)
		return;

	const ImGuiTableColumnSortSpecs *spec = &sort_specs->Specs[0];

	std::stable_sort(results.begin(), results.end(), [spec](const TorrentSearchResult &a, const TorrentSearchResult &b)
					 {
						int result = 0;
						switch (spec->ColumnIndex)
						{
						case 0: // Name
							result = a.name.compare(b.name);
							break;
						case 1: // Size
							if (a.sizeBytes < b.sizeBytes) result = -1;
							else if (a.sizeBytes > b.sizeBytes) result = 1;
							else result = 0;
							break;
						case 2: // Seeds
							if (a.seeders < b.seeders) result = -1;
							else if (a.seeders > b.seeders) result = 1;
							else result = 0;
							break;
						case 3: // Leechers
							if (a.leechers < b.leechers) result = -1;
							else if (a.leechers > b.leechers) result = 1;
							else result = 0;
							break;
						case 4: // Ratio
						{
							float ratioA = (a.leechers > 0) ? (float)a.seeders / a.leechers : (a.seeders > 0 ? 1000.0f : 0.0f);
							float ratioB = (b.leechers > 0) ? (float)b.seeders / b.leechers : (b.seeders > 0 ? 1000.0f : 0.0f);

							if (ratioA < ratioB) result = -1;
							else if (ratioA > ratioB) result = 1;
							else result = 0;
							break;
						}
						case 5: // Completed
							if (a.completed < b.completed) result = -1;
							else if (a.completed > b.completed) result = 1;
							else result = 0;
							break;
						case 6: // Created
							if (a.createdUnix < b.createdUnix) result = -1;
							else if (a.createdUnix > b.createdUnix) result = 1;
							else result = 0;
							break;
						case 7: // Last Seen
							if (a.scrapedDate < b.scrapedDate) result = -1;
							else if (a.scrapedDate > b.scrapedDate) result = 1;
							else result = 0;
							break;
						case 8: // Category
							result = a.category.compare(b.category);
							break;
						default:
							result = 0;
							break;
						}

						// Apply sort direction
						if (spec->SortDirection == ImGuiSortDirection_Descending) {
							result = -result;
						}

						return result < 0; });

	sort_specs->SpecsDirty = false;
}

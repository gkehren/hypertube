#include "SearchUI.hpp"
#include "StringUtils.hpp"
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
	// Process any pending results from async search
	processPendingResults();

	// Search input section with styled header
	HypertubeTheme::drawSectionHeader("Torrent Search");

	// Provider selection dropdown
	displayProviderSelection();

	ImGui::Spacing();

	// Search input with better styling
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 8.0f));
	ImGui::PushItemWidth(-160.0f); // Leave space for search button
	bool enterPressed = ImGui::InputText("##search", searchQueryBuffer, sizeof(searchQueryBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();
	ImGui::PopStyleVar(2);

	ImGui::SameLine();
	bool searchClicked = HypertubeTheme::drawStyledButton("Search", ImVec2(140, 32), true);

	if (enterPressed || searchClicked)
	{
		performSearch(std::string(searchQueryBuffer));
	}

	ImGui::Spacing();

	// Direct download section
	displayDirectDownload();

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
			if (sort_specs->SpecsDirty)
			{
				sortTorrentResults(searchResults, sort_specs);
			}
		}

		for (int i = 0; i < (int)searchResults.size(); ++i)
		{
			displayEnhancedSearchResultRow(searchResults[i], i);
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

		for (int i = 0; i < (int)favoritesDisplay.size(); ++i)
		{
			displayFavoriteRow(favoritesDisplay[i], i);
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
	char sizeBuf[64];
	Utils::formatBytes(result.sizeBytes, false, sizeBuf, sizeof(sizeBuf));
	ImGui::Text("%s", sizeBuf);

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
	char createdDate[32];
	formatUnixTime(result.createdUnix, createdDate, sizeof(createdDate));
	ImGui::Text("%s", createdDate);

	// Last seen (scraped date)
	ImGui::TableSetColumnIndex(7);
	ImGui::AlignTextToFramePadding();
	char scrapedDate[32];
	formatUnixTime(result.scrapedDate, scrapedDate, sizeof(scrapedDate));
	ImGui::Text("%s", scrapedDate);

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
	char sizeBuf[64];
	Utils::formatBytes(result.sizeBytes, false, sizeBuf, sizeof(sizeBuf));
	ImGui::Text("%s", sizeBuf);

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
	char createdDate[32];
	formatUnixTime(result.createdUnix, createdDate, sizeof(createdDate));
	ImGui::Text("%s", createdDate);

	// Last seen (scraped date)
	ImGui::TableSetColumnIndex(7);
	ImGui::AlignTextToFramePadding();
	char scrapedDate[32];
	formatUnixTime(result.scrapedDate, scrapedDate, sizeof(scrapedDate));
	ImGui::Text("%s", scrapedDate);

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

	isSearching = true;
	searchResults.clear();
	currentSearchQuery = query;
	nextToken.clear();
	hasMoreResults = true;

	SearchQuery searchQuery(query);

	// Launch async search
	searchEngine.searchTorrentsAsync(searchQuery,
											 [this](Result result, SearchResponse response)
											 {
												 // This callback runs in worker thread, so we need to store results safely
												 std::lock_guard<std::mutex> lock(resultsMutex);
												 pendingResult = result;
												 pendingResponse = response;
												 hasPendingResults = true;
											 });
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
		isSearching = true;
		SearchQuery searchQuery(currentSearchQuery, 0, nextToken);

		// Launch async search for more results
		searchEngine.searchTorrentsAsync(searchQuery,
												 [this](Result result, SearchResponse response)
												 {
													 // This callback runs in worker thread
													 std::lock_guard<std::mutex> lock(resultsMutex);
													 pendingResult = result;
													 pendingResponse = response;
													 hasPendingResults = true;
												 });
	}
}

void SearchUI::formatUnixTime(int64_t unixTime, char *buffer, size_t bufferSize)
{
	if (bufferSize == 0)
	{
		return;
	}

	if (unixTime == 0)
	{
		snprintf(buffer, bufferSize, "N/A");
		return;
	}

	// Handle both seconds and milliseconds timestamps
	std::time_t time;
	if (unixTime > 1000000000000LL)
	{ // If timestamp is in milliseconds (> year 2001 in ms)
		time = static_cast<std::time_t>(unixTime / 1000);
	}
	else
	{
		time = static_cast<std::time_t>(unixTime);
	}

	// Validate the timestamp is reasonable (between 1970 and 2100)
	if (time < 0 || time > 4102444800)
	{ // 4102444800 = 2100-01-01
		snprintf(buffer, bufferSize, "Invalid TS");
		return;
	}

	// Use localtime for local time
	std::tm tm_buf = {};
	if (!Utils::SystemUtils::getLocalTime(time, tm_buf))
	{
		snprintf(buffer, bufferSize, "TM Error");
		return;
	}

	// Use manual formatting
	if (std::strftime(buffer, bufferSize, "%Y-%m-%d", &tm_buf) == 0)
	{
		snprintf(buffer, bufferSize, "Format Error");
	}
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
	// Check if we have pending results from async search (must run in UI thread)
	std::lock_guard<std::mutex> lock(resultsMutex);

	if (hasPendingResults)
	{
		isSearching = false;

		if (!pendingResult.has_value() || !pendingResult.value())
		{
			std::string errorMsg = pendingResult.has_value() ? pendingResult.value().message : "Unknown error";

			// Don't show error popup for cancelled searches
			if (errorMsg.find("cancelled") == std::string::npos)
			{
				if (onShowFailurePopup)
				{
					onShowFailurePopup("Search failed: " + errorMsg);
				}
			}
		}
		else
		{
			// Check if this is a "load more" request by seeing if we already have results
			bool isLoadMore = !searchResults.empty() && !pendingResponse.torrents.empty();

			if (isLoadMore)
			{
				// Append new results
				searchResults.insert(searchResults.end(),
									 pendingResponse.torrents.begin(),
									 pendingResponse.torrents.end());
			}
			else
			{
				// Replace results (new search)
				searchResults = pendingResponse.torrents;
			}

			nextToken = pendingResponse.nextToken;
			hasMoreResults = pendingResponse.hasMore;
		}

		hasPendingResults = false;
		pendingResult.reset();
	}
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

void SearchUI::displayProviderSelection()
{
	// Get available providers
	auto providers = searchEngine.getAvailableProviders();
	std::string currentProvider = searchEngine.getActiveProviderName();

	// Find the index of the current provider
	for (size_t i = 0; i < providers.size(); ++i)
	{
		if (providers[i] == currentProvider)
		{
			selectedProviderIndex = static_cast<int>(i);
			break;
		}
	}

	ImGui::Text("Search Provider:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);

	// Create combo box
	if (ImGui::BeginCombo("##provider", currentProvider.c_str()))
	{
		for (size_t i = 0; i < providers.size(); ++i)
		{
			bool isSelected = (selectedProviderIndex == static_cast<int>(i));
			if (ImGui::Selectable(providers[i].c_str(), isSelected))
			{
				selectedProviderIndex = static_cast<int>(i);
				searchEngine.setActiveProvider(providers[i]);

				// Save provider preference to configuration
				if (configManager)
				{
					searchEngine.saveProviderToConfig(*configManager);
				}
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

void SearchUI::displayDirectDownload()
{
	// Direct download section
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Direct Download");
	ImGui::Spacing();

	// Input field for magnet links, .torrent URLs, or HTTP/HTTPS downloads
	ImGui::PushItemWidth(-160.0f);
	bool enterPressedDirect = ImGui::InputTextWithHint(
		"##directdownload",
		"Enter magnet link, .torrent URL, or HTTP/HTTPS download link",
		directDownloadBuffer,
		sizeof(directDownloadBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	bool addClicked = HypertubeTheme::drawStyledButton("Add", ImVec2(140, 0), true);

	if (enterPressedDirect || addClicked)
	{
		std::string input(directDownloadBuffer);
		if (!input.empty())
		{
			handleDirectDownload(input);
			// Clear the buffer after adding
			directDownloadBuffer[0] = '\0';
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
}

void SearchUI::handleDirectDownload(const std::string &input)
{
	// Check if it's a magnet link
	if (input.find("magnet:?xt=urn:btih:") == 0)
	{
		// It's a magnet link - create a pseudo search result and handle it
		TorrentSearchResult result;
		result.magnetUri = input;
		result.name = "Direct Magnet Download";

		// Try to extract infohash from magnet link
		size_t btihPos = input.find("btih:");
		if (btihPos != std::string::npos)
		{
			size_t start = btihPos + 5;
			size_t end = input.find('&', start);
			if (end == std::string::npos)
			{
				end = input.length();
			}
			result.infoHash = input.substr(start, end - start);
		}

		// Try to extract name from magnet link
		size_t dnPos = input.find("&dn=");
		if (dnPos != std::string::npos)
		{
			size_t start = dnPos + 4;
			size_t end = input.find('&', start);
			if (end == std::string::npos)
			{
				end = input.length();
			}
			std::string encodedName = input.substr(start, end - start);
			// Simple URL decode (replace + with space, %20 with space)
			for (size_t i = 0; i < encodedName.length(); ++i)
			{
				if (encodedName[i] == '+')
				{
					encodedName[i] = ' ';
				}
			}
			size_t pos = 0;
			while ((pos = encodedName.find("%20", pos)) != std::string::npos)
			{
				encodedName.replace(pos, 3, " ");
				pos += 1;
			}
			result.name = encodedName;
		}

		result.sizeBytes = 0;
		result.seeders = 0;
		result.leechers = 0;
		result.category = "Direct";

		handleSearchResultSelection(result);
	}
	// Check if it's a .torrent URL
	else if (input.find("http://") == 0 || input.find("https://") == 0)
	{
		// Check if it's a .torrent file
		if (input.find(".torrent") != std::string::npos)
		{
			// Create a pseudo search result for torrent file URL
			TorrentSearchResult result;
			result.magnetUri = input; // Store URL in magnetUri field
			result.name = "Direct Torrent File Download";
			result.infoHash = ""; // Will be determined after download
			result.sizeBytes = 0;
			result.seeders = 0;
			result.leechers = 0;
			result.category = "Direct";

			handleSearchResultSelection(result);
		}
		else
		{
			// It's a direct HTTP/HTTPS download
			if (onShowFailurePopup)
			{
				onShowFailurePopup("Direct HTTP/HTTPS downloads are not yet supported. Please use magnet links or .torrent files.");
			}
		}
	}
	else
	{
		// Unknown format
		if (onShowFailurePopup)
		{
			onShowFailurePopup("Invalid input. Please enter a magnet link (magnet:?xt=urn:btih:...) or a .torrent URL (http://.../.torrent)");
		}
	}
}


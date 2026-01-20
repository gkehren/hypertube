#include "SearchUI.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstring>

SearchUI::SearchUI(SearchEngine &searchEngine)
	: searchEngine(searchEngine)
{
}

void SearchUI::displayIntegratedSearch()
{
	// Search input section
	ImGui::Text("Search for Torrents:");
	ImGui::Separator();

	// Search input with better styling
	ImGui::Text("Query:");
	ImGui::SameLine();
	ImGui::PushItemWidth(-150.0f); // Leave space for search button
	bool enterPressed = ImGui::InputText("##search", searchQueryBuffer, sizeof(searchQueryBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	bool searchClicked = ImGui::Button("Search", ImVec2(140, 0));

	if (enterPressed || searchClicked)
	{
		performSearch(std::string(searchQueryBuffer));
	}

	// Show search status
	if (isSearching)
	{
		ImGui::Text("Searching...");
		ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(0.0f, 0.0f), "");
	}

	ImGui::Separator();

	// Display search results if available
	if (!searchResults.empty() || !currentSearchQuery.empty())
	{
		displayEnhancedSearchResults();
	}
	else if (!isSearching)
	{
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Enter a search query to find torrents...");
	}
}

void SearchUI::displaySearchWindow()
{
	if (!ImGui::Begin("Torrent Search", &showSearchWindow, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	// Search input
	ImGui::Text("Search Query:");
	ImGui::SameLine();
	if (ImGui::InputText("##search", searchQueryBuffer, sizeof(searchQueryBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		performSearch(std::string(searchQueryBuffer));
	}

	ImGui::SameLine();
	if (ImGui::Button("Search"))
	{
		performSearch(std::string(searchQueryBuffer));
	}

	if (isSearching)
	{
		ImGui::Text("Searching...");
		ImGui::ProgressBar(-1.0f * ImGui::GetTime());
	}

	// Display search results
	if (!searchResults.empty())
	{
		displaySearchResults();
	}

	ImGui::End();
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
							  ImGuiTableFlags_RowBg,
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
				if (sort_specs->SpecsCount > 0)
				{
					const ImGuiTableColumnSortSpecs *spec = &sort_specs->Specs[0];

					std::stable_sort(searchResults.begin(), searchResults.end(), [spec](const TorrentSearchResult &a, const TorrentSearchResult &b)
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
				}
				sort_specs->SpecsDirty = false;
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

void SearchUI::displaySearchResults()
{
	ImGui::Separator();
	ImGui::Text("Search Results (%d found):", (int)searchResults.size());

	// Pagination controls at the top
	ImGui::PushID("top_pagination");
	displayPaginationControls();
	ImGui::PopID();

	// Create a table for search results
	if (ImGui::BeginTable("SearchResultsTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0, 300)))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("Seeders", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Leechers", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Completed", ImGuiTableColumnFlags_WidthFixed, 70);
		ImGui::TableSetupColumn("Created", ImGuiTableColumnFlags_WidthFixed, 90);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableHeadersRow();

		for (int i = 0; i < (int)searchResults.size(); ++i)
		{
			displaySearchResultRow(searchResults[i], i);
		}

		ImGui::EndTable();
	}

	// Pagination controls at the bottom
	ImGui::PushID("bottom_pagination");
	displayPaginationControls();
	ImGui::PopID();
}

void SearchUI::displayEnhancedSearchResultRow(const TorrentSearchResult &result, int index)
{
	ImGui::TableNextRow();
	ImGui::PushID(index);

	// Name column with truncation for very long names
	ImGui::TableSetColumnIndex(0);
	std::string displayName = result.name;
	if (displayName.length() > 60)
	{
		displayName = displayName.substr(0, 57) + "...";
	}

	if (ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
	{
		handleSearchResultSelection(result);
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
	ImGui::Text("%s", formatBytes(result.sizeBytes, false).c_str());

	// Seeders with color coding
	ImGui::TableSetColumnIndex(2);
	if (result.seeders >= 10)
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d", result.seeders);
	else if (result.seeders >= 1)
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%d", result.seeders);
	else
		ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d", result.seeders);

	// Leechers
	ImGui::TableSetColumnIndex(3);
	ImGui::Text("%d", result.leechers);

	// Seed/Leech ratio
	ImGui::TableSetColumnIndex(4);
	if (result.leechers > 0)
	{
		float ratio = (float)result.seeders / result.leechers;
		if (ratio >= 2.0f)
			ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%.1f", ratio);
		else if (ratio >= 1.0f)
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%.1f", ratio);
		else
			ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%.1f", ratio);
	}
	else if (result.seeders > 0)
	{
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "∞");
	}
	else
	{
		ImGui::Text("-");
	}

	// Completed count
	ImGui::TableSetColumnIndex(5);
	if (result.completed > 0)
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d", result.completed);
	else
		ImGui::Text("%d", result.completed);

	// Created date
	ImGui::TableSetColumnIndex(6);
	char createdDate[32];
	formatUnixTime(result.createdUnix, createdDate, sizeof(createdDate));
	ImGui::Text("%s", createdDate);

	// Last seen (scraped date)
	ImGui::TableSetColumnIndex(7);
	char scrapedDate[32];
	formatUnixTime(result.scrapedDate, scrapedDate, sizeof(scrapedDate));
	ImGui::Text("%s", scrapedDate);

	// Category
	ImGui::TableSetColumnIndex(8);
	ImGui::Text("%s", result.category.c_str());

	// Action button with improved styling
	ImGui::TableSetColumnIndex(9);
	if (ImGui::Button("Download", ImVec2(-1, 0)))
	{
		handleSearchResultSelection(result);
	}

	ImGui::PopID();
}

void SearchUI::displaySearchResultRow(const TorrentSearchResult &result, int index)
{
	ImGui::TableNextRow();

	// Name
	ImGui::TableSetColumnIndex(0);
	ImGui::Text("%s", result.name.c_str());

	// Size
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%s", formatBytes(result.sizeBytes, false).c_str());

	// Seeders
	ImGui::TableSetColumnIndex(2);
	if (result.seeders > 0)
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d", result.seeders);
	else
		ImGui::Text("%d", result.seeders);

	// Leechers
	ImGui::TableSetColumnIndex(3);
	ImGui::Text("%d", result.leechers);

	// Completed
	ImGui::TableSetColumnIndex(4);
	if (result.completed > 0)
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d", result.completed);
	else
		ImGui::Text("%d", result.completed);

	// Created date
	ImGui::TableSetColumnIndex(5);
	char createdDate[32];
	formatUnixTime(result.createdUnix, createdDate, sizeof(createdDate));
	ImGui::Text("%s", createdDate);

	// Category
	ImGui::TableSetColumnIndex(6);
	ImGui::Text("%s", result.category.c_str());

	// Action button
	ImGui::TableSetColumnIndex(7);
	std::string buttonId = "Add##" + std::to_string(index);
	if (ImGui::Button(buttonId.c_str()))
	{
		handleSearchResultSelection(result);
	}
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
	if (query.empty() || isSearching)
		return;

	isSearching = true;
	searchResults.clear();
	currentSearchQuery = query;
	nextToken.clear();
	hasMoreResults = true;

	SearchResponse response;
	SearchQuery searchQuery(query);
	Result result = searchEngine.searchTorrents(searchQuery, response);

	isSearching = false;

	if (!result)
	{
		if (onShowFailurePopup)
		{
			onShowFailurePopup("Search failed: " + result.message);
		}
	}
	else
	{
		searchResults = response.torrents;
		nextToken = response.nextToken;
		hasMoreResults = response.hasMore;
	}
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
	if (hasMoreResults && !isSearching && !currentSearchQuery.empty() && !nextToken.empty())
	{
		isSearching = true;
		SearchResponse response;

		SearchQuery searchQuery(currentSearchQuery, 0, nextToken);
		Result result = searchEngine.searchTorrents(searchQuery, response);

		isSearching = false;

		if (result && !response.torrents.empty())
		{
			// Append new results to existing ones
			searchResults.insert(searchResults.end(), response.torrents.begin(), response.torrents.end());
			nextToken = response.nextToken;
			hasMoreResults = response.hasMore;
		}
		else if (!result)
		{
			if (onShowFailurePopup)
			{
				onShowFailurePopup("Failed to load more results: " + result.message);
			}
		}
		else
		{
			// No more results
			hasMoreResults = false;
		}
	}
}

std::string SearchUI::formatBytes(size_t bytes, bool speed)
{
	const char *units[] = {"B", "KB", "MB", "GB", "TB"};
	size_t size = bytes;
	size_t unitIndex = 0;

	while (size >= 1024 && unitIndex < sizeof(units) / sizeof(units[0]) - 1)
	{
		size /= 1024;
		unitIndex++;
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
	if (speed)
		oss << "/s";
	return oss.str();
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
	std::tm *tm = std::localtime(&time);

	if (!tm)
	{
		snprintf(buffer, bufferSize, "TM Error");
		return;
	}

	// Use manual formatting
	if (std::strftime(buffer, bufferSize, "%Y-%m-%d", tm) == 0)
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
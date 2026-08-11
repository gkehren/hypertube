#pragma once

#include "presentation/UiDtos.hpp"
#include "Result.hpp"
#include "TorrentManager.hpp"
#include "presentation/TorrentAvailability.hpp"

#include <optional>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Presentation
{
enum class TorrentSortField
{
	Queue,
	Name,
	Size,
	Progress,
	Status,
	DownloadRate,
	UploadRate,
	Eta,
	Seeds,
	Peers
};

class TorrentListPresenter
{
public:
	explicit TorrentListPresenter(TorrentManager &torrentManager);

	void setCategoryFilter(int filter);
	int categoryFilter() const { return categoryFilter_; }
	void setTextFilter(std::string filter);
	const std::string &textFilter() const { return textFilter_; }
	void setSort(TorrentSortField field, bool ascending);
	void setSelectedId(std::string id);
	const std::string &selectedId() const { return selectedId_; }

	std::vector<TorrentRowDto> buildRows();
	std::vector<CategoryDto> buildCategories();
	std::optional<TorrentRowDto> findRowById(const std::string &id);

	Result executeCommand(const std::string &id, TorrentCommand command);
	Result removeTorrent(const std::string &id, TorrentRemovalMode mode);
	std::optional<lt::info_hash_t> hashForId(const std::string &id) const;
	TorrentAvailabilityInfo availabilityForId(const std::string &id);
	std::size_t registrySize() const { ensurePresentationCurrent(); return hashesById_.size(); }
	std::uint64_t collectionRevision() const { return torrentManager.getTorrentCollectionRevision(); }
	static std::string idForHash(const lt::info_hash_t &hash);

private:
	struct PresentationSnapshot
	{
		std::uint64_t collectionRevision = 0;
		std::uint64_t statusRevision = 0;
		std::vector<TorrentRowDto> allRows;
		std::unordered_map<std::string, lt::info_hash_t> hashesById;
		std::array<int, 7> categoryCounts{};
	};

	TorrentManager &torrentManager;
	int categoryFilter_ = 0;
	TorrentSortField sortField_ = TorrentSortField::Queue;
	bool sortAscending_ = true;
	std::string textFilter_;
	std::string selectedId_;
	mutable std::unordered_map<std::string, lt::info_hash_t> hashesById_;
	mutable PresentationSnapshot presentation_;
	mutable bool presentationValid_ = false;

	const std::vector<TorrentRowDto> &buildUnfilteredRows();
	void ensurePresentationCurrent() const;
	static bool matchesCategory(const TorrentRowDto &row, int filter);
	bool matchesCategory(const TorrentRowDto &row) const;
	bool matchesTextFilter(const TorrentRowDto &row) const;
};
} // namespace Presentation

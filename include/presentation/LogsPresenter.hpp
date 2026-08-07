#pragma once

#include "Logger.hpp"
#include "presentation/UiDtos.hpp"
#include <libtorrent/alert.hpp>

#include <cstddef>
#include <string>
#include <vector>

class TorrentManager;

namespace Presentation
{
class LogsPresenter
{
public:
	explicit LogsPresenter(TorrentManager &torrentManager);

	void update();
	void clear();
	void setMaxEntries(std::size_t maxEntries) { maxEntries_ = maxEntries; }
	std::size_t maxEntries() const { return maxEntries_; }
	void setLevelEnabled(Utils::LogLevel level, bool enabled);
	bool levelEnabled(Utils::LogLevel level) const;
	std::vector<LogRowDto> buildRows() const;

private:
	TorrentManager &torrentManager;
	std::size_t maxEntries_ = 1000;
	bool showDebug_ = false;
	bool showInfo_ = true;
	bool showWarnings_ = true;
	bool showErrors_ = true;

	void processAlert(lt::alert *alert);
	void addLogEntry(const std::string &category, const std::string &message, Utils::LogLevel level);
};
} // namespace Presentation

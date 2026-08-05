#pragma once

#include "SearchEngine.hpp"
#include <optional>
#include <string>

enum class TorrentAddSource
{
	TorrentFile,
	Magnet,
	SearchResult
};

struct TorrentAddRequest
{
	TorrentAddSource source;
	std::string value;
};

class TorrentAddFlow
{
public:
	void beginFile(std::string path) { begin(TorrentAddSource::TorrentFile, std::move(path)); }
	void beginMagnet(std::string uri) { begin(TorrentAddSource::Magnet, std::move(uri)); }
	void beginSearchResult(const TorrentSearchResult &result) { begin(TorrentAddSource::SearchResult, result.magnetUri); }
	bool needsDestinationDialog() const { return pending.has_value() && !dialogOpened; }
	void markDestinationDialogOpened() { dialogOpened = pending.has_value(); }
	bool hasPendingRequest() const { return pending.has_value(); }
	std::optional<TorrentAddRequest> take()
	{
		auto request = std::move(pending);
		cancel();
		return request;
	}
	void cancel()
	{
		pending.reset();
		dialogOpened = false;
	}

private:
	void begin(TorrentAddSource source, std::string value)
	{
		cancel();
		if (!value.empty())
			pending = TorrentAddRequest{source, std::move(value)};
	}

	std::optional<TorrentAddRequest> pending;
	bool dialogOpened = false;
};

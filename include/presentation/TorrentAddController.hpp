#pragma once

#include "ui/TorrentAddFlow.hpp"

class TorrentAddController
{
public:
	void beginFile(const std::string &path) { flow_.beginFile(path); }
	void beginMagnet(const std::string &magnetUri) { flow_.beginMagnet(magnetUri); }
	void beginSearchResult(const TorrentSearchResult &result) { flow_.beginSearchResult(result); }
	bool needsDestinationDialog() const { return flow_.needsDestinationDialog(); }
	void markDestinationDialogOpened() { flow_.markDestinationDialogOpened(); }
	bool hasPendingRequest() const { return flow_.hasPendingRequest(); }
	std::optional<TorrentAddRequest> take() { return flow_.take(); }
	void cancel() { flow_.cancel(); }

private:
	TorrentAddFlow flow_;
};

#pragma once

#include <imgui.h>
#include <ImGuiFileDialog.h>
#include <ImGuiFileDialogConfig.h>
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include "TorrentManager.hpp"
#include "SearchEngine.hpp"
#include "TorrentAddFlow.hpp"

struct TorrentRemovalInfo
{
	lt::info_hash_t hash;
	std::string name;
	TorrentRemovalMode removeMode;

	TorrentRemovalInfo(const lt::info_hash_t &hash, std::string name, TorrentRemovalMode removeMode)
		: hash(hash), name(std::move(name)), removeMode(removeMode) {}
};

class ModalDialogs
{
public:
	ModalDialogs(TorrentManager &torrentManager);
	~ModalDialogs() = default;

	// Modal display methods
	void handleAddTorrentModal(bool &showTorrentPopup, const std::string &defaultSavePath);
	void handleAddMagnetTorrentModal(bool &showMagnetTorrentPopup);
	void handleRemoveTorrentModal(const std::vector<TorrentRemovalInfo> &torrentsToRemove);
	void handleAskSavePathModal(const std::string &defaultSavePath, const std::string &savePath);

	// Individual modal methods
	void askSavePathModal();
	void renderPopupFailure(const std::string &message);
	void removeTorrentModal();

	// State management
	void beginSearchResult(const TorrentSearchResult &result) { addFlow.beginSearchResult(result); }

	char *getMagnetLinkBuffer() { return magnetLinkBuffer; }
	const char *getMagnetLinkBuffer() const { return magnetLinkBuffer; }

	// Callback setup
	void setShowFailurePopupCallback(std::function<void(const std::string &)> callback);
	void setTorrentAddCallback(std::function<void(const std::string &, const std::string &, bool)> callback);
	void setRemoveCompletedCallback(std::function<void()> callback);
	void setRemoveCancelledCallback(std::function<void()> callback);

	// Access for external state
	void setSavePath(const std::string &path) { savePath = path; }
	void cancelPendingAdd() { addFlow.cancel(); }

private:
	TorrentManager &torrentManager;

	// Modal state
	char magnetLinkBuffer[4096] = {0};
	TorrentAddFlow addFlow;
	std::string savePath;
	std::vector<TorrentRemovalInfo> pendingRemovals;

	// Callbacks
	std::function<void(const std::string &)> onShowFailurePopup;
	std::function<void(const std::string &, const std::string &, bool)> onTorrentAdd; // path, savePath, isMagnet
	std::function<void()> onRemoveCompleted;
	std::function<void()> onRemoveCancelled;
};

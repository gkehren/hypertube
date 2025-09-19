#pragma once

#include <imgui.h>
#include <ImGuiFileDialog.h>
#include <ImGuiFileDialogConfig.h>
#include <string>
#include <vector>
#include <functional>
#include "TorrentManager.hpp"
#include "SearchEngine.hpp"

struct TorrentRemovalInfo;

class ModalDialogs
{
public:
	ModalDialogs(TorrentManager &torrentManager);
	~ModalDialogs() = default;

	// Modal display methods
	void handleAddTorrentModal(bool &showTorrentPopup, const std::string &defaultSavePath);
	void handleAddMagnetTorrentModal(bool &showMagnetTorrentPopup);
	void handleRemoveTorrentModal(const std::vector<TorrentRemovalInfo> &torrentsToRemove);
	void handleAskSavePathModal(const std::pair<bool, std::string> &torrentToAdd,
								const TorrentSearchResult &selectedSearchResult,
								const std::string &defaultSavePath,
								const std::string &savePath);

	// Individual modal methods
	void askSavePathModal();
	void renderPopupFailure(const std::string &message);
	void removeTorrentModal();

	// State management
	std::pair<bool, std::string> getTorrentToAdd() const { return torrentToAdd; }
	void clearTorrentToAdd() { torrentToAdd = std::make_pair(false, ""); }

	char *getMagnetLinkBuffer() { return magnetLinkBuffer; }
	const char *getMagnetLinkBuffer() const { return magnetLinkBuffer; }

	// Callback setup
	void setShowFailurePopupCallback(std::function<void(const std::string &)> callback);
	void setTorrentAddCallback(std::function<void(const std::string &, const std::string &, bool)> callback);
	void setRemoveCompletedCallback(std::function<void()> callback);

	// Access for external state
	void setSavePath(const std::string &path) { savePath = path; }
	void setSelectedSearchResult(const TorrentSearchResult &result) { selectedSearchResult = result; }
	void clearSelectedSearchResult() { selectedSearchResult = TorrentSearchResult(); }

private:
	TorrentManager &torrentManager;

	// Modal state
	char magnetLinkBuffer[4096] = {0};
	std::pair<bool, std::string> torrentToAdd;
	std::string savePath;
	TorrentSearchResult selectedSearchResult;

	// Callbacks
	std::function<void(const std::string &)> onShowFailurePopup;
	std::function<void(const std::string &, const std::string &, bool)> onTorrentAdd; // path, savePath, isMagnet
	std::function<void()> onRemoveCompleted;
};
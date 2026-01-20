#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuiFileDialog.h>
#include <ImGuiFileDialogConfig.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include "TorrentManager.hpp"
#include "SearchEngine.hpp"
#include "Result.hpp"
#include "TorrentTableUI.hpp"
#include "TorrentDetailsUI.hpp"
#include "SearchUI.hpp"
#include "ModalDialogs.hpp"
#include "ConfigManager.hpp"

class UIManager;

struct MenuItem
{
	std::string label;
	std::string shortcut;
	std::function<void(UIManager *)> action;
};

struct TorrentRemovalInfo
{
	lt::sha1_hash hash;
	RemoveTorrentType removeType;

	TorrentRemovalInfo(const lt::sha1_hash &hash, RemoveTorrentType removeType)
		: hash(hash), removeType(removeType) {}
};

class UIManager
{
public:
	UIManager(TorrentManager &torrentManager, SearchEngine &searchEngine, ConfigManager &configManager);
	~UIManager() = default;

	void init(GLFWwindow *window);
	void initImGui(GLFWwindow *window);
	void setDefaultSavePath();
	void renderFrame(GLFWwindow *window, const ImVec4 &clear_color);
	void shutdown();
	const ImGuiIO &getIO() const;
	bool shouldExit() const;

private:
	ImGuiIO io;
	bool exitRequested = false;
	std::string defaultSavePath;

	// UI Component instances
	std::unique_ptr<TorrentTableUI> torrentTableUI;
	std::unique_ptr<TorrentDetailsUI> torrentDetailsUI;
	std::unique_ptr<SearchUI> searchUI;
	std::unique_ptr<ModalDialogs> modalDialogs;

	// Failure popup state
	bool showFailurePopup = false;
	std::string failurePopupMessage;

	// Torrent removal state
	std::vector<TorrentRemovalInfo> torrentsToRemove;

	// Preferences dialog state
	bool showPreferencesDialog = false;
	bool showTorrentPopup = false;
	bool showMagnetTorrentPopup = false;
	int tempDownloadSpeedLimit = 0;
	int tempUploadSpeedLimit = 0;

	TorrentManager &torrentManager;
	SearchEngine &searchEngine;
	ConfigManager &configManager;

	// Core UI methods
	void displayCategories();
	void displayTorrentManagement();
	void displayMenuBar();
	void setupDocking(ImGuiID dockspace_id);

	// Helper methods
	void handleModals();
	void setupUICallbacks();
	void showFailurePopupWithMessage(const std::string &message);
	void handleTorrentRemoval();
	void displayPreferencesDialog();
	void loadSpeedLimitsFromConfig();
	void applySpeedLimits();
};
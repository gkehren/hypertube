#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuiFileDialog.h>
#include <ImGuiFileDialogConfig.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include "torrent_manager.hpp"
#include "Result.hpp"

struct	TorrentRemovalInfo
{
	lt::sha1_hash		hash;
	RemoveTorrentType	removeType;

	TorrentRemovalInfo(const lt::sha1_hash& hash, RemoveTorrentType removeType)
		: hash(hash), removeType(removeType) {}
};

class UI
{
	public:
		void				init(GLFWwindow* window);
		void				render();
		void				shutdown();
		const ImGuiIO&		getIO() const;
		bool				shouldExit() const;
		void				setAddTorrentCallback(std::function<Result(const std::string&)> callback);
		void				setAddMagnetLinkCallback(std::function<Result(const std::string&)> callback);
		void				setGetTorrentsCallback(std::function<const std::unordered_map<lt::sha1_hash, lt::torrent_handle>&()> callback);
		void				setRemoveTorrentCallback(std::function<Result(const lt::sha1_hash, RemoveTorrentType)> callback);

	private:
		ImGuiIO				io;
		char				magnetLinkBuffer[4096] = { 0 };

		bool				exitRequested = false;
		lt::torrent_handle	selectedTorrent;
		bool				showFailurePopup = false;
		std::string			failurePopupMessage;
		std::vector<TorrentRemovalInfo>	torrentsToRemove;

		void				displayTorrentList();
		void				displayTorrentDetails();

		// Layout Management
		void				saveLayout(const std::string &configFilePath);
		void				loadLayout();
		void				resetLayout();

		// Modal Windows
		void				addTorrentModal();
		void				addMagnetTorrentModal();
		void				renderPopupFailure(const std::string& message);
		void				removeTorrentModal();

		// Callbacks
		std::function<Result(const std::string&)>										addTorrentCallback;
		std::function<Result(const std::string&)>										addMagnetLinkCallback;
		std::function<const std::unordered_map<lt::sha1_hash, lt::torrent_handle>&()>	getTorrentsCallback;
		std::function<Result(const lt::sha1_hash, RemoveTorrentType)>					removeTorrentCallback;
};

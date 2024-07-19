#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include "torrent_manager.hpp"

class UI
{
	public:
		void				init(GLFWwindow* window);
		void				render();
		void				shutdown();
		const ImGuiIO&		getIO() const;
		bool				shouldExit() const;
		void				setAddMagnetLinkCallback(std::function<void(const std::string&)> callback);
		void				setGetTorrentsCallback(std::function<std::unordered_map<lt::sha1_hash, lt::torrent_handle>&()> callback);
		void				setRemoveTorrentCallback(std::function<void(const lt::sha1_hash, RemoveTorrentType)> callback);

	private:
		ImGuiIO				io;
		char				magnetLinkBuffer[4096] = { 0 };

		bool				exitRequested = false;
		lt::torrent_handle	selectedTorrent;

		void				displayTorrentList();
		void				displayTorrentDetails();

		// Layout Management
		void				saveLayout(const std::string &configFilePath);
		void				loadLayout();
		void				resetLayout();

		// Modal Windows
		void				addTorrentModal();
		void				addMagnetTorrentModal();

		// Callbacks
		std::function<void(const std::string&)>									addMagnetLinkCallback;
		std::function<std::unordered_map<lt::sha1_hash, lt::torrent_handle>&()>	getTorrentsCallback;
		std::function<void(const lt::sha1_hash, RemoveTorrentType)>				removeTorrentCallback;
};

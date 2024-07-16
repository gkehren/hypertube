#pragma once

#include "ui.hpp"
#include "config_manager.hpp"
#include "torrent_manager.hpp"

class MainWindow
{
	public:
		MainWindow(ConfigManager *configManager, TorrentManager *torrentManager);
		~MainWindow();
		void show();

	private:
		GLFWwindow*	window;
		ConfigManager	*configManager;
		TorrentManager	*torrentManager;
		UI			ui;
};

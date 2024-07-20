#pragma once

#include "ui.hpp"
#include "config_manager.hpp"
#include "torrent_manager.hpp"

class App
{
	public:
		App();
		~App();

		void	run();

	private:
		GLFWwindow*		window;
		ConfigManager	configManager;
		TorrentManager	torrentManager;
		UI				ui;
};

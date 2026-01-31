#pragma once

#include "UIManager.hpp"
#include "ConfigManager.hpp"
#include "TorrentManager.hpp"
#include "SearchEngine.hpp"

class App
{
public:
	App();
	~App();

	void run();

private:
	GLFWwindow *window;
	ConfigManager torrentsConfigManager;
	ConfigManager settingsConfigManager;
	TorrentManager torrentManager;
	SearchEngine searchEngine;
	UIManager uiManager;
};

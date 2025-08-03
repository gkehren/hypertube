#pragma once

#include "UIManager.hpp"
#include "ConfigManager.hpp"
#include "TorrentManager.hpp"

class App
{
public:
	App();
	~App();

	void run();

private:
	GLFWwindow *window;
	ConfigManager configManager;
	TorrentManager torrentManager;
	UIManager uiManager;
};

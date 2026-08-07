#pragma once

#include "ConfigManager.hpp"
#include "TorrentManager.hpp"
#include "SearchEngine.hpp"
#include "SystemUtils.hpp"

class App
{
public:
	App();
	~App();

	void initialize();
	void shutdown();

	ConfigManager &torrentsConfigManager() { return torrentsConfigManager_; }
	ConfigManager &settingsConfigManager() { return settingsConfigManager_; }
	TorrentManager &torrentManager() { return torrentManager_; }
	SearchEngine &searchEngine() { return searchEngine_; }
	Utils::SystemUtils::SystemOpener &systemOpener() { return systemOpener_; }

private:
	ConfigManager torrentsConfigManager_;
	ConfigManager settingsConfigManager_;
	TorrentManager torrentManager_;
	SearchEngine searchEngine_;
	Utils::SystemUtils::SystemOpener systemOpener_;
	bool initialized_ = false;
};

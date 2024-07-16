#include "app.hpp"
#include "ui/main_window.hpp"

void	App::run() {
	ConfigManager	configManager;
	configManager.load("config.json");
	TorrentManager	torrentManager;
	MainWindow	mainWindow(&configManager, &torrentManager);
	mainWindow.show();
}

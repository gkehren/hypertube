#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <json.hpp>
#include "TorrentManager.hpp"

using json = nlohmann::json;

// Forward declarations
struct TorrentSearchResult;

struct TorrentConfigData
{
	std::string magnetUri;
	std::string savePath;
	std::string torrentFilePath;
};

class ConfigManager
{
public:
	void load(const std::string &path, bool fullConfig = true);
	void save(const std::string &path);

	void saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &torrents, const std::unordered_map<lt::sha1_hash, std::string> &torrentFilePaths);
	std::vector<TorrentConfigData> loadTorrents(const std::string &path);

	// Favorites and search history
	void saveFavoritesAndHistory(const std::vector<TorrentSearchResult> &favorites, const std::vector<std::string> &searchHistory);
	void loadFavoritesAndHistory(std::vector<TorrentSearchResult> &favorites, std::vector<std::string> &searchHistory);

	// Speed limit configuration
	void setDownloadSpeedLimit(int bytesPerSecond);
	void setUploadSpeedLimit(int bytesPerSecond);
	int getDownloadSpeedLimit() const;
	int getUploadSpeedLimit() const;

	// New settings configuration
	void setDownloadPath(const std::string &path);
	std::string getDownloadPath() const;
	void setEnableDHT(bool enable);
	bool getEnableDHT() const;
	void setEnableUPnP(bool enable);
	bool getEnableUPnP() const;
	void setEnableNATPMP(bool enable);
	bool getEnableNATPMP() const;

	// Schema management
	int getConfigVersion() const;
	void ensureDefaultConfig();
	void migrateConfig(int fromVersion, int toVersion);
	// Theme configuration
	void setTheme(int themeIndex);
	int getTheme() const;

	json &getConfig();

private:
	json config;
	static constexpr int CURRENT_CONFIG_VERSION = 1;

	json createDefaultConfig() const;
	void ensureSettingsStructure();
};

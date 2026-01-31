#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <json.hpp>
#include "TorrentManager.hpp"

using json = nlohmann::json;

struct TorrentConfigData
{
	std::string magnetUri;
	std::string savePath;
	std::string torrentFilePath;
};

class ConfigManager
{
public:
	void load(const std::string &path);
	void save(const std::string &path);

	void saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &torrents, const std::unordered_map<lt::sha1_hash, std::string> &torrentFilePaths);
	std::vector<TorrentConfigData> loadTorrents(const std::string &path);

	// Speed limit configuration
	void setDownloadSpeedLimit(int bytesPerSecond);
	void setUploadSpeedLimit(int bytesPerSecond);
	int getDownloadSpeedLimit() const;
	int getUploadSpeedLimit() const;

	// Theme configuration
	void setTheme(int themeIndex);
	int getTheme() const;

	json &getConfig();

private:
	json config;
};

#pragma once

#include <string>
#include <json.hpp>
#include "TorrentManager.hpp"

using json = nlohmann::json;

class ConfigManager
{
public:
	void load(const std::string &path);
	void save(const std::string &path);

	void saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &torrents);
	const std::vector<std::string> loadTorrents(const std::string &path);

	json &getConfig();

private:
	json config;
};

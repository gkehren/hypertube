#include "ConfigManager.hpp"
#include <fstream>

void ConfigManager::load(const std::string &path)
{
	std::ifstream file(path);
	if (file.is_open())
	{
		file >> this->config;
		file.close();
	}
}

void ConfigManager::save(const std::string &path)
{
	std::ofstream file(path);
	if (file.is_open())
	{
		file << config.dump(4);
		file.close();
	}
}

void ConfigManager::saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &torrents)
{
	json torrentsJson;
	for (const auto &[hash, handle] : torrents)
	{
		lt::torrent_status status = handle.status(lt::torrent_handle::query_save_path | lt::torrent_handle::query_name);
		std::string magnetUri = lt::make_magnet_uri(handle);
		std::string savePath = status.save_path;
		torrentsJson.push_back({{"magnet_uri", magnetUri},
								{"save_path", savePath}});
	}
	this->config["torrents"] = torrentsJson;
	save("./config/torrents.json");
}

std::vector<std::string> ConfigManager::loadTorrents(const std::string &path)
{
	std::vector<std::string> torrents;
	const auto &torrentsJson = this->config["torrents"];
	torrents.reserve(torrentsJson.size() * 2);
	for (const auto &torrent : torrentsJson)
	{
		std::string magnetUri = torrent["magnet_uri"];
		std::string savePath = torrent["save_path"];
		torrents.push_back(std::move(magnetUri));
		torrents.push_back(std::move(savePath));
	}
	return torrents;
}

json &ConfigManager::getConfig()
{
	return this->config;
}

void ConfigManager::setDownloadSpeedLimit(int bytesPerSecond)
{
	config["speed_limits"]["download"] = bytesPerSecond;
}

void ConfigManager::setUploadSpeedLimit(int bytesPerSecond)
{
	config["speed_limits"]["upload"] = bytesPerSecond;
}

int ConfigManager::getDownloadSpeedLimit() const
{
	if (config.contains("speed_limits") && config["speed_limits"].contains("download"))
	{
		return config["speed_limits"]["download"];
	}
	return 0; // 0 means unlimited
}

int ConfigManager::getUploadSpeedLimit() const
{
	if (config.contains("speed_limits") && config["speed_limits"].contains("upload"))
	{
		return config["speed_limits"]["upload"];
	}
	return 0; // 0 means unlimited
}

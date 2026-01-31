#include "ConfigManager.hpp"
#include <fstream>
#include <iostream>

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

void ConfigManager::saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &torrents, const std::unordered_map<lt::sha1_hash, std::string> &torrentFilePaths)
{
	json torrentsJson;
	for (const auto &[hash, handle] : torrents)
	{
		lt::torrent_status status = handle.status(lt::torrent_handle::query_save_path | lt::torrent_handle::query_name);
		std::string magnetUri = lt::make_magnet_uri(handle);
		std::string savePath = status.save_path;

		json torrentEntry = {
			{"magnet_uri", magnetUri},
			{"save_path", savePath}};

		auto it = torrentFilePaths.find(hash);
		if (it != torrentFilePaths.end())
		{
			torrentEntry["torrent_path"] = it->second;
		}

		torrentsJson.push_back(torrentEntry);
	}
	this->config["torrents"] = torrentsJson;
	save("./config/torrents.json");
}

std::vector<TorrentConfigData> ConfigManager::loadTorrents(const std::string &path)
{
	std::vector<TorrentConfigData> torrents;

	json sourceConfig;
	if (!path.empty())
	{
		std::ifstream file(path);
		if (file.is_open())
		{
			try
			{
				file >> sourceConfig;
			}
			catch (const std::exception &)
			{
				sourceConfig = json{};
			}
			file.close();
		}
	}

	if (sourceConfig.is_null() || sourceConfig.empty())
	{
		sourceConfig = this->config;
	}

	if (!sourceConfig.contains("torrents"))
		return torrents;

	const auto &torrentsJson = sourceConfig["torrents"];
	torrents.reserve(torrentsJson.size());
	for (const auto &torrent : torrentsJson)
	{
		TorrentConfigData data;
		if (torrent.contains("magnet_uri"))
			data.magnetUri = torrent["magnet_uri"];
		if (torrent.contains("save_path"))
			data.savePath = torrent["save_path"];
		if (torrent.contains("torrent_path"))
			data.torrentFilePath = torrent["torrent_path"];

		torrents.push_back(std::move(data));
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

#include "config_manager.hpp"
#include <fstream>

void	ConfigManager::load(const std::string& path)
{
	std::ifstream	file(path);
	if (file.is_open())
	{
		file >> this->config;
		file.close();
	}
}

void	ConfigManager::save(const std::string& path)
{
	std::ofstream	file(path);
	if (file.is_open())
	{
		file << config.dump(4);
		file.close();
	}
}

void	ConfigManager::saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle>& torrents)
{
	json	torrentsJson;
	for (const auto& [hash, handle] : torrents)
	{
		lt::torrent_status status = handle.status(lt::torrent_handle::query_save_path | lt::torrent_handle::query_name);
		std::string	magnetUri = lt::make_magnet_uri(handle);
		std::string savePath = status.save_path;
		torrentsJson.push_back({
			{ "magnet_uri", magnetUri },
			{ "save_path", savePath }
		});
	}
	this->config["torrents"] = torrentsJson;
	save("./config/torrents.json");
}

const std::vector<std::string>	ConfigManager::loadTorrents(const std::string& path)
{
	std::vector<std::string>	torrents;
	json	torrentsJson = this->config["torrents"];
	for (const auto& torrent : torrentsJson)
	{
		std::string	magnetUri = torrent["magnet_uri"];
		std::string	savePath = torrent["save_path"];
		torrents.push_back(magnetUri);
		torrents.push_back(savePath);
	}
	return torrents;
}

json&	ConfigManager::getConfig()
{
	return this->config;
}

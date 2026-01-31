#include "ConfigManager.hpp"
#include <fstream>
#include <iostream>
#include <sys/stat.h>

json ConfigManager::createDefaultConfig() const
{
	json defaultConfig = {
		{"version", CURRENT_CONFIG_VERSION},
		{"settings", {
			{"speed_limits", {
				{"download", 0},
				{"upload", 0}
			}},
			{"download_path", "~/Downloads"},
			{"enable_dht", true},
			{"enable_upnp", true},
			{"enable_natpmp", true}
		}}
	};
	return defaultConfig;
}

void ConfigManager::load(const std::string &path)
{
	std::ifstream file(path);
	if (file.is_open())
	{
		try
		{
			file >> this->config;
			file.close();
			
			// Check if config needs migration
			int currentVersion = getConfigVersion();
			if (currentVersion < CURRENT_CONFIG_VERSION)
			{
				migrateConfig(currentVersion, CURRENT_CONFIG_VERSION);
			}
			
			// Ensure all default settings exist
			ensureDefaultConfig();
		}
		catch (const std::exception &e)
		{
			std::cerr << "Error loading config: " << e.what() << std::endl;
			this->config = createDefaultConfig();
		}
	}
	else
	{
		// No config file exists, create default
		this->config = createDefaultConfig();
	}
}

void ConfigManager::save(const std::string &path)
{
	std::ofstream file(path);
	if (file.is_open())
	{
		// Ensure version is always present before saving
		if (!config.contains("version"))
		{
			config["version"] = CURRENT_CONFIG_VERSION;
		}
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
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	if (!config["settings"].contains("speed_limits"))
	{
		config["settings"]["speed_limits"] = json::object();
	}
	config["settings"]["speed_limits"]["download"] = bytesPerSecond;
}

void ConfigManager::setUploadSpeedLimit(int bytesPerSecond)
{
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	if (!config["settings"].contains("speed_limits"))
	{
		config["settings"]["speed_limits"] = json::object();
	}
	config["settings"]["speed_limits"]["upload"] = bytesPerSecond;
}

int ConfigManager::getDownloadSpeedLimit() const
{
	if (config.contains("speed_limits") && config["speed_limits"].contains("download"))
	{
		return config["speed_limits"]["download"];
	}
	if (config.contains("settings") && config["settings"].contains("speed_limits") && 
	    config["settings"]["speed_limits"].contains("download"))
	{
		return config["settings"]["speed_limits"]["download"];
	}
	return 0; // 0 means unlimited
}

int ConfigManager::getUploadSpeedLimit() const
{
	if (config.contains("speed_limits") && config["speed_limits"].contains("upload"))
	{
		return config["speed_limits"]["upload"];
	}
	if (config.contains("settings") && config["settings"].contains("speed_limits") && 
	    config["settings"]["speed_limits"].contains("upload"))
	{
		return config["settings"]["speed_limits"]["upload"];
	}
	return 0; // 0 means unlimited
}

void ConfigManager::setDownloadPath(const std::string &path)
{
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	config["settings"]["download_path"] = path;
}

std::string ConfigManager::getDownloadPath() const
{
	if (config.contains("settings") && config["settings"].contains("download_path"))
	{
		return config["settings"]["download_path"];
	}
	return "~/Downloads";
}

void ConfigManager::setEnableDHT(bool enable)
{
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	config["settings"]["enable_dht"] = enable;
}

bool ConfigManager::getEnableDHT() const
{
	if (config.contains("settings") && config["settings"].contains("enable_dht"))
	{
		return config["settings"]["enable_dht"];
	}
	return true; // Default to enabled
}

void ConfigManager::setEnableUPnP(bool enable)
{
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	config["settings"]["enable_upnp"] = enable;
}

bool ConfigManager::getEnableUPnP() const
{
	if (config.contains("settings") && config["settings"].contains("enable_upnp"))
	{
		return config["settings"]["enable_upnp"];
	}
	return true; // Default to enabled
}

void ConfigManager::setEnableNATPMP(bool enable)
{
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	config["settings"]["enable_natpmp"] = enable;
}

bool ConfigManager::getEnableNATPMP() const
{
	if (config.contains("settings") && config["settings"].contains("enable_natpmp"))
	{
		return config["settings"]["enable_natpmp"];
	}
	return true; // Default to enabled
}

int ConfigManager::getConfigVersion() const
{
	if (config.contains("version"))
	{
		return config["version"];
	}
	return 0; // Version 0 means unversioned/legacy config
}

void ConfigManager::ensureDefaultConfig()
{
	json defaults = createDefaultConfig();
	
	// Ensure version is set
	if (!config.contains("version"))
	{
		config["version"] = CURRENT_CONFIG_VERSION;
	}
	
	// Ensure settings section exists
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
	
	// Add any missing default settings
	for (auto &[key, value] : defaults["settings"].items())
	{
		if (!config["settings"].contains(key))
		{
			config["settings"][key] = value;
		}
	}
}

void ConfigManager::migrateConfig(int fromVersion, int toVersion)
{
	std::cout << "Migrating config from version " << fromVersion << " to " << toVersion << std::endl;
	
	if (fromVersion == 0 && toVersion >= 1)
	{
		// Migration from unversioned to version 1
		// Wrap existing config in "settings" if not already wrapped
		if (!config.contains("settings"))
		{
			json oldConfig = config;
			config = createDefaultConfig();
			
			// Preserve old speed_limits if they exist
			if (oldConfig.contains("speed_limits"))
			{
				config["settings"]["speed_limits"] = oldConfig["speed_limits"];
			}
			
			// Preserve old download_path if it exists
			if (oldConfig.contains("download_path"))
			{
				config["settings"]["download_path"] = oldConfig["download_path"];
			}
			
			// Preserve old DHT/UPnP settings if they exist
			if (oldConfig.contains("enable_dht"))
			{
				config["settings"]["enable_dht"] = oldConfig["enable_dht"];
			}
			if (oldConfig.contains("enable_upnp"))
			{
				config["settings"]["enable_upnp"] = oldConfig["enable_upnp"];
			}
			if (oldConfig.contains("enable_natpmp"))
			{
				config["settings"]["enable_natpmp"] = oldConfig["enable_natpmp"];
			}
			
			// Preserve torrents list if it exists
			if (oldConfig.contains("torrents"))
			{
				config["torrents"] = oldConfig["torrents"];
			}
		}
		
		config["version"] = 1;
	}
	
	// Future migrations can be added here
	// if (fromVersion == 1 && toVersion >= 2) { ... }
}

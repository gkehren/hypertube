#include "ConfigManager.hpp"
#include <fstream>
#include <iostream>

Result ConfigManager::load(const std::string &path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		// If file doesn't exist, use default config
		applyDefaultConfig();
		return Result::Success();
	}

	try
	{
		file >> this->config;
		file.close();

		// Validate the loaded config
		if (!validateConfig())
		{
			applyDefaultConfig();
			return Result::Failure("Invalid configuration detected. Using default values.");
		}

		return Result::Success();
	}
	catch (const json::parse_error &e)
	{
		file.close();
		applyDefaultConfig();
		return Result::Failure("Failed to parse configuration file: " + std::string(e.what()) + ". Using default values.");
	}
	catch (const std::exception &e)
	{
		file.close();
		applyDefaultConfig();
		return Result::Failure("Error loading configuration: " + std::string(e.what()) + ". Using default values.");
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

Result ConfigManager::loadTorrents(const std::string &path, std::vector<TorrentConfigData> &outTorrents)
{
	outTorrents.clear();

	json sourceConfig;
	if (!path.empty())
	{
		std::ifstream file(path);
		if (file.is_open())
		{
			try
			{
				file >> sourceConfig;
				file.close();
			}
			catch (const json::parse_error &e)
			{
				file.close();
				return Result::Failure("Failed to parse torrents configuration: " + std::string(e.what()));
			}
			catch (const std::exception &e)
			{
				file.close();
				return Result::Failure("Error loading torrents configuration: " + std::string(e.what()));
			}
		}
	}

	if (sourceConfig.is_null() || sourceConfig.empty())
	{
		sourceConfig = this->config;
	}

	if (!sourceConfig.contains("torrents"))
		return Result::Success(); // No torrents to load is not an error

	try
	{
		const auto &torrentsJson = sourceConfig["torrents"];
		if (!torrentsJson.is_array())
		{
			return Result::Failure("Invalid torrents configuration: 'torrents' must be an array");
		}

		outTorrents.reserve(torrentsJson.size());
		for (const auto &torrent : torrentsJson)
		{
			TorrentConfigData data;
			if (torrent.contains("magnet_uri") && torrent["magnet_uri"].is_string())
				data.magnetUri = torrent["magnet_uri"];
			if (torrent.contains("save_path") && torrent["save_path"].is_string())
				data.savePath = torrent["save_path"];
			if (torrent.contains("torrent_path") && torrent["torrent_path"].is_string())
				data.torrentFilePath = torrent["torrent_path"];

			outTorrents.push_back(std::move(data));
		}
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		outTorrents.clear();
		return Result::Failure("Error processing torrents: " + std::string(e.what()));
	}
}

json &ConfigManager::getConfig()
{
	return this->config;
}

void ConfigManager::setDownloadSpeedLimit(int bytesPerSecond)
{
	// Validate: must be non-negative
	if (bytesPerSecond < 0)
		bytesPerSecond = 0;
	
	config["speed_limits"]["download"] = bytesPerSecond;
}

void ConfigManager::setUploadSpeedLimit(int bytesPerSecond)
{
	// Validate: must be non-negative
	if (bytesPerSecond < 0)
		bytesPerSecond = 0;
	
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

void ConfigManager::applyDefaultConfig()
{
	config = {
		{"speed_limits", {
			{"download", 0},
			{"upload", 0}
		}},
		{"download_path", "/default/downloads"}
	};
}

bool ConfigManager::validateConfig()
{
	// If config is empty or not an object, it's invalid
	if (config.is_null() || !config.is_object())
	{
		return false;
	}

	// Validate speed_limits if present
	if (config.contains("speed_limits"))
	{
		const auto &speedLimits = config["speed_limits"];
		if (!speedLimits.is_object())
		{
			return false;
		}

		// Check download limit is a valid number if present
		if (speedLimits.contains("download"))
		{
			if (!speedLimits["download"].is_number_integer())
			{
				return false;
			}
			int downloadLimit = speedLimits["download"];
			if (downloadLimit < 0)
			{
				return false;
			}
		}

		// Check upload limit is a valid number if present
		if (speedLimits.contains("upload"))
		{
			if (!speedLimits["upload"].is_number_integer())
			{
				return false;
			}
			int uploadLimit = speedLimits["upload"];
			if (uploadLimit < 0)
			{
				return false;
			}
		}
	}

	return true;
}

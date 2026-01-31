#include "ConfigManager.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

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
		// File will be automatically closed by destructor (RAII)

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
		applyDefaultConfig();
		return Result::Failure("Failed to parse configuration file: " + std::string(e.what()) + ". Using default values.");
	}
	catch (const std::exception &e)
	{
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
			}
			catch (const json::parse_error &e)
			{
				return Result::Failure("Failed to parse torrents configuration: " + std::string(e.what()));
			}
			catch (const std::exception &e)
			{
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
	// Validate: must be non-negative (0 means unlimited)
	config["speed_limits"]["download"] = std::max(bytesPerSecond, 0);
}

void ConfigManager::setUploadSpeedLimit(int bytesPerSecond)
{
	// Validate: must be non-negative (0 means unlimited)
	config["speed_limits"]["upload"] = std::max(bytesPerSecond, 0);
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
	// Use platform-appropriate default download path
	// Note: This is just a fallback default. The actual download path
	// is typically managed by UIManager::setDefaultSavePath()
	std::string default_path;
#ifdef _WIN32
	const char* user_profile = std::getenv("USERPROFILE");
	// Validate that the environment variable was retrieved successfully
	if (user_profile && std::string(user_profile).length() > 0) {
		default_path = std::string(user_profile) + "\\Downloads";
	} else {
		// Safe fallback that should exist on all Windows systems
		default_path = "C:\\Users\\Public\\Downloads";
	}
#else
	const char* home_dir = std::getenv("HOME");
	// Validate that the environment variable was retrieved successfully
	if (home_dir && std::string(home_dir).length() > 0) {
		default_path = std::string(home_dir) + "/Downloads";
	} else {
		// Use XDG_DOWNLOAD_DIR standard location as fallback
		default_path = "/var/tmp";
	}
#endif

	config = {
		{"speed_limits", {
			{"download", 0},
			{"upload", 0}
		}},
		{"download_path", default_path}
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

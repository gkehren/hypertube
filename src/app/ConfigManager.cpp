#include "ConfigManager.hpp"
#include "SearchEngine.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

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

Result ConfigManager::load(const std::string &path, bool fullConfig)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		// If file doesn't exist, use default config
		this->config = fullConfig ? createDefaultConfig() : json{{"torrents", json::array()}};
		return Result::Success();
	}

	try
	{
		file >> this->config;
		// File will be automatically closed by destructor (RAII)

		if (fullConfig)
		{
			// Validate the loaded config
			if (!validateConfig())
			{
				this->config = createDefaultConfig();
				return Result::Failure("Invalid configuration detected. Using default values.");
			}

			// Check if config needs migration
			int currentVersion = getConfigVersion();
			if (currentVersion < CURRENT_CONFIG_VERSION)
			{
				migrateConfig(currentVersion, CURRENT_CONFIG_VERSION);
			}
			// Ensure all default settings exist
			ensureDefaultConfig();
		}

		return Result::Success();
	}
	catch (const json::parse_error &e)
	{
		this->config = fullConfig ? createDefaultConfig() : json{{"torrents", json::array()}};
		return Result::Failure("Failed to parse configuration file: " + std::string(e.what()) + ". Using default values.");
	}
	catch (const std::exception &e)
	{
		this->config = fullConfig ? createDefaultConfig() : json{{"torrents", json::array()}};
		return Result::Failure("Error loading configuration: " + std::string(e.what()) + ". Using default values.");
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
		// File will be automatically closed by destructor (RAII)
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

	json torrentsFile = {{"torrents", torrentsJson}};
	std::ofstream file("./config/torrents.json");
	if (file.is_open())
	{
		file << torrentsFile.dump(4);
		// File will be automatically closed by destructor (RAII)
	}
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

void ConfigManager::ensureSettingsStructure()
{
	if (!config.contains("settings"))
	{
		config["settings"] = json::object();
	}
}

void ConfigManager::setDownloadSpeedLimit(int bytesPerSecond)
{
	// Validate: must be non-negative (0 means unlimited)
	ensureSettingsStructure();
	if (!config["settings"].contains("speed_limits"))
	{
		config["settings"]["speed_limits"] = json::object();
	}
	config["settings"]["speed_limits"]["download"] = std::max(bytesPerSecond, 0);
}

void ConfigManager::setUploadSpeedLimit(int bytesPerSecond)
{
	// Validate: must be non-negative (0 means unlimited)
	ensureSettingsStructure();
	if (!config["settings"].contains("speed_limits"))
	{
		config["settings"]["speed_limits"] = json::object();
	}
	config["settings"]["speed_limits"]["upload"] = std::max(bytesPerSecond, 0);
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

<<<<<<< HEAD
void ConfigManager::setDownloadPath(const std::string &path)
{
	ensureSettingsStructure();
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
	ensureSettingsStructure();
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
	ensureSettingsStructure();
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
	ensureSettingsStructure();
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

void ConfigManager::saveFavoritesAndHistory(const std::vector<TorrentSearchResult> &favorites, const std::vector<std::string> &searchHistory)
{
	// Save favorites
	json favoritesJson = json::array();
	for (const auto &fav : favorites)
	{
		json favEntry = {
			{"name", fav.name},
			{"magnet_uri", fav.magnetUri},
			{"info_hash", fav.infoHash},
			{"size_bytes", fav.sizeBytes},
			{"seeders", fav.seeders},
			{"leechers", fav.leechers},
			{"date_uploaded", fav.dateUploaded},
			{"category", fav.category},
			{"created_unix", fav.createdUnix},
			{"scraped_date", fav.scrapedDate},
			{"completed", fav.completed}};
		favoritesJson.push_back(favEntry);
	}
	config["favorites"] = favoritesJson;

	// Save search history
	config["search_history"] = searchHistory;

	// Save to file
	save("./config/settings.json");
}

void ConfigManager::loadFavoritesAndHistory(std::vector<TorrentSearchResult> &favorites, std::vector<std::string> &searchHistory)
{
	favorites.clear();
	searchHistory.clear();

	// Load favorites
	if (config.contains("favorites") && config["favorites"].is_array())
	{
		for (const auto &favJson : config["favorites"])
		{
			TorrentSearchResult fav;
			if (favJson.contains("name"))
				fav.name = favJson["name"];
			if (favJson.contains("magnet_uri"))
				fav.magnetUri = favJson["magnet_uri"];
			if (favJson.contains("info_hash"))
				fav.infoHash = favJson["info_hash"];
			if (favJson.contains("size_bytes"))
				fav.sizeBytes = favJson["size_bytes"];
			if (favJson.contains("seeders"))
				fav.seeders = favJson["seeders"];
			if (favJson.contains("leechers"))
				fav.leechers = favJson["leechers"];
			if (favJson.contains("date_uploaded"))
				fav.dateUploaded = favJson["date_uploaded"];
			if (favJson.contains("category"))
				fav.category = favJson["category"];
			if (favJson.contains("created_unix"))
				fav.createdUnix = favJson["created_unix"];
			if (favJson.contains("scraped_date"))
				fav.scrapedDate = favJson["scraped_date"];
			if (favJson.contains("completed"))
				fav.completed = favJson["completed"];
			favorites.push_back(fav);
		}
	}

	// Load search history
	if (config.contains("search_history") && config["search_history"].is_array())
	{
		for (const auto &historyItem : config["search_history"])
		{
			if (historyItem.is_string())
			{
				searchHistory.push_back(historyItem);
			}
		}
	}
}

void ConfigManager::setTheme(int themeIndex)
{
	config["theme"] = themeIndex;
}

int ConfigManager::getTheme() const
{
	if (config.contains("theme"))
	{
		// Handle both string (legacy) and number formats
		if (config["theme"].is_string())
		{
			std::string themeStr = config["theme"];
			if (themeStr == "dark")
				return 0;
			if (themeStr == "ocean")
				return 1;
			if (themeStr == "nord")
				return 2;
			if (themeStr == "dracula")
				return 3;
			if (themeStr == "cyberpunk")
				return 4;
			return 0; // Default to dark if unknown
		}
		else if (config["theme"].is_number())
		{
			return config["theme"];
		}
	}
	return 0; // Default to Dark theme
}

void ConfigManager::applyDefaultConfig()
{
	// Use platform-appropriate default download path
	// Note: This is just a fallback default. The actual download path
	// is typically managed by UIManager::setDefaultSavePath()
	std::string default_path;
#ifdef _WIN32
	const char* user_profile = std::getenv("USERPROFILE");
	// Validate that the environment variable was retrieved successfully and is non-empty
	if (user_profile && user_profile[0] != '\0') {
		default_path = std::string(user_profile) + "\\Downloads";
	} else {
		// Safe fallback that should exist on all Windows systems
		default_path = "C:\\Users\\Public\\Downloads";
	}
#else
	const char* home_dir = std::getenv("HOME");
	// Validate that the environment variable was retrieved successfully and is non-empty
	if (home_dir && home_dir[0] != '\0') {
		default_path = std::string(home_dir) + "/Downloads";
	} else {
		// Use XDG_DOWNLOAD_DIR standard location as fallback
		default_path = "/var/tmp";
	}
#endif

	json defaultConfig = createDefaultConfig();
	// Update the download path in the default config with the platform-appropriate path
	defaultConfig["settings"]["download_path"] = default_path;
	config = defaultConfig;
}

bool ConfigManager::validateConfig()
{
	// If config is empty or not an object, it's invalid
	if (config.is_null() || !config.is_object())
	{
		return false;
	}

	// Validate speed_limits if present (check both old and new structure)
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

	// Validate settings.speed_limits if present (new structure)
	if (config.contains("settings") && config["settings"].is_object())
	{
		const auto &settings = config["settings"];
		if (settings.contains("speed_limits") && settings["speed_limits"].is_object())
		{
			const auto &speedLimits = settings["speed_limits"];
			
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
	}

	return true;
}

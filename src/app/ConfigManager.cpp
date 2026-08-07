#include "ConfigManager.hpp"
#include "SearchEngine.hpp"
#include "AppPaths.hpp"
#include "Logger.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <unordered_map>

namespace
{
std::string encodeHex(const std::vector<char> &data)
{
	static constexpr char digits[] = "0123456789abcdef";
	std::string encoded;
	encoded.reserve(data.size() * 2);
	for (const unsigned char byte : data)
	{
		encoded.push_back(digits[byte >> 4]);
		encoded.push_back(digits[byte & 0x0f]);
	}
	return encoded;
}

bool decodeHex(const std::string &encoded, std::vector<char> &data)
{
	static constexpr std::size_t maxResumeDataSize = 16 * 1024 * 1024;
	if (encoded.size() % 2 != 0 || encoded.size() / 2 > maxResumeDataSize)
		return false;
	auto value = [](char character) -> int
	{
		if (character >= '0' && character <= '9') return character - '0';
		if (character >= 'a' && character <= 'f') return character - 'a' + 10;
		if (character >= 'A' && character <= 'F') return character - 'A' + 10;
		return -1;
	};
	data.clear();
	data.reserve(encoded.size() / 2);
	for (std::size_t index = 0; index < encoded.size(); index += 2)
	{
		const int high = value(encoded[index]);
		const int low = value(encoded[index + 1]);
		if (high < 0 || low < 0)
		{
			data.clear();
			return false;
		}
		data.push_back(static_cast<char>((high << 4) | low));
	}
	return true;
}

void applyMissingDefaults(json &target, const json &defaults)
{
	if (!target.is_object() || !defaults.is_object())
		return;
	for (const auto &[key, value] : defaults.items())
	{
		if (!target.contains(key))
			target[key] = value;
		else if (target[key].is_object() && value.is_object())
			applyMissingDefaults(target[key], value);
	}
}

bool writeJsonAtomically(const std::string &path, const json &data, std::string &errorMessage)
{
	const std::filesystem::path target(path);
	std::error_code error;
	if (!target.parent_path().empty())
		std::filesystem::create_directories(target.parent_path(), error);
	if (error)
	{
		errorMessage = "Unable to create configuration directory: " + error.message();
		return false;
	}

	const std::filesystem::path temporary = target.string() + ".tmp";
	{
		std::ofstream file(temporary, std::ios::trunc);
		if (!file.is_open())
		{
			errorMessage = "Unable to open temporary configuration file";
			return false;
		}
		file << data.dump(4) << '\n';
		file.flush();
		if (!file.good())
		{
			errorMessage = "Unable to flush temporary configuration file";
			std::filesystem::remove(temporary, error);
			return false;
		}
	}

	if (std::filesystem::exists(target, error))
	{
		const std::filesystem::path backup = target.string() + ".bak";
		std::filesystem::copy_file(target, backup, std::filesystem::copy_options::overwrite_existing, error);
		if (error)
		{
			errorMessage = "Unable to create configuration backup: " + error.message();
			std::filesystem::remove(temporary, error);
			return false;
		}
	}

	std::filesystem::rename(temporary, target, error);
	if (error)
	{
		// Windows does not replace an existing file with rename(). The backup
		// above makes this fallback recoverable while preserving the same API.
		std::filesystem::remove(target, error);
		error.clear();
		std::filesystem::rename(temporary, target, error);
	}
	if (error)
	{
		errorMessage = "Unable to replace configuration file: " + error.message();
		std::filesystem::remove(temporary, error);
		return false;
	}
	return true;
}

void applyPreferencesToJson(json &config, const PreferencesSettings &settings)
{
	if (!config.is_object())
		config = json::object();
	if (!config.contains("version"))
		config["version"] = ConfigManager::CURRENT_CONFIG_VERSION;
	config["theme"] = settings.theme;
	if (!config.contains("settings") || !config["settings"].is_object())
		config["settings"] = json::object();
	auto &target = config["settings"];
	target["speed_limits"]["download"] = std::max(settings.downloadSpeedLimit, 0);
	target["speed_limits"]["upload"] = std::max(settings.uploadSpeedLimit, 0);
	target["download_path"] = settings.downloadPath;
	target["enable_dht"] = settings.enableDht;
	target["enable_upnp"] = settings.enableUpnp;
	target["enable_natpmp"] = settings.enableNatPmp;
	target["search"]["torznab_enabled"] = settings.torznabEnabled;
	target["search"]["torznab_url"] = settings.torznabUrl;
	target["proxy"]["enabled"] = settings.proxyEnabled;
	target["proxy"]["type"] = settings.proxyType == "http" ? "http" : "socks5";
	target["proxy"]["host"] = settings.proxyHost;
	target["proxy"]["port"] = std::clamp(settings.proxyPort, 1, 65535);
	target["proxy"]["username"] = settings.proxyUsername;
	config["ui"] = {
		{"sidebar_width", std::clamp(settings.ui.sidebarWidth, 120, 600)},
		{"bottom_panel_height", std::clamp(settings.ui.bottomPanelHeight, 120, 1000)},
		{"sidebar_collapsed", settings.ui.sidebarCollapsed},
		{"selected_main_tab", std::max(settings.ui.selectedMainTab, 0)},
		{"selected_details_tab", std::max(settings.ui.selectedDetailsTab, 0)}};
}
} // namespace

ConfigManager::ConfigManager()
{
	saveThread = std::thread(&ConfigManager::workerLoop, this);
}

ConfigManager::~ConfigManager()
{
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		stopWorker = true;
	}
	queueCv.notify_one();
	if (saveThread.joinable())
	{
		saveThread.join();
	}
}

void ConfigManager::workerLoop()
{
	while (true)
	{
		SaveRequest req;
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			queueCv.wait(lock, [this] { return stopWorker || !saveQueue.empty(); });

			if (saveQueue.empty() && stopWorker)
			{
				return;
			}

			if (!saveQueue.empty())
			{
				req = std::move(saveQueue.front());
				saveQueue.pop();
			}
			else
			{
				continue;
			}
		}

		// Perform I/O outside the queue lock. A temporary file and backup keep
		// the last valid configuration usable after an interruption or crash.
		std::string errorMessage;
		Result result = Result::Success();
		if (!writeJsonAtomically(req.path, req.data, errorMessage))
		{
			std::cerr << "Failed to save configuration '" << req.path << "': " << errorMessage << std::endl;
			Utils::Logger::error("config", "Failed to save '" + req.path + "': " + errorMessage);
			result = Result::Failure(errorMessage, ResultCode::Storage, true);
		}
		if (req.completion)
			req.completion->set_value(result);

		activeJobs--;
		queueCv.notify_all();
	}
}

SaveHandle ConfigManager::enqueueSave(const std::string& path, json data)
{
	auto completion = std::make_shared<std::promise<Result>>();
	SaveHandle handle = completion->get_future().share();
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		activeJobs++;
		saveQueue.push({path, std::move(data), completion});
	}
	queueCv.notify_one();
	return handle;
}

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
			{"enable_natpmp", true},
			{"search", {
				{"torznab_enabled", false},
				{"torznab_url", "http://127.0.0.1:9696/api/v1/indexer/all/results/torznab/api"}
			}},
			{"proxy", {
				{"enabled", false},
				{"type", "socks5"},
				{"host", "127.0.0.1"},
				{"port", 1080},
				{"username", ""}
			}}
		}},
		{"ui", {
			{"sidebar_width", 240},
			{"bottom_panel_height", 300},
			{"sidebar_collapsed", false},
			{"selected_main_tab", 0},
			{"selected_details_tab", 0}
		}}
	};
	return defaultConfig;
}

Result ConfigManager::load(const std::string &path, bool fullConfig)
{
	json loadedConfig;
	const auto defaultConfig = [&]()
	{
		std::lock_guard<std::mutex> lock(configMutex);
		config = fullConfig ? createDefaultConfig() : json{{"torrents", json::array()}};
	};

	bool fileFound = false;
	bool loaded = false;
	std::string loadError;
	std::vector<std::filesystem::path> candidates;
	if (!path.empty())
	{
		candidates.emplace_back(path);
		candidates.emplace_back(std::filesystem::path(path).string() + ".bak");
	}
	for (const auto &candidate : candidates)
	{
		std::ifstream file(candidate);
		if (!file.is_open())
			continue;
		fileFound = true;
		try
		{
			json candidateConfig;
			file >> candidateConfig;

			if (fullConfig)
			{
				// Validate each candidate before accepting it. A syntactically valid
				// primary file can still be corrupt at the schema/value level, in
				// which case the backup must get a chance to recover the state.
				std::lock_guard<std::mutex> lock(configMutex);
				json previousConfig = config;
				config = candidateConfig;
				try
				{
					if (!validateConfig())
					{
						config = std::move(previousConfig);
						loadError = "Invalid configuration in: " + candidate.string();
						Utils::Logger::warning("config", loadError);
						continue;
					}
				}
				catch (...)
				{
					config = std::move(previousConfig);
					throw;
				}
				config = std::move(previousConfig);
			}

			loadedConfig = std::move(candidateConfig);
			loaded = true;
			if (candidate.extension() == ".bak")
				Utils::Logger::warning("config", "Loading backup configuration: " + candidate.string());
			break;
		}
		catch (const json::parse_error &e)
		{
			loadError = "Failed to parse configuration file: " + std::string(e.what());
		}
		catch (const std::exception &e)
		{
			loadError = "Error loading configuration: " + std::string(e.what());
		}
	}

	if (!loaded)
	{
		defaultConfig();
		if (!fileFound)
			return Result::Success();
		Utils::Logger::error("config", loadError + ". Using default values.");
		return Result::Failure(loadError + ". Using default values.");
	}

	try
	{
		std::lock_guard<std::mutex> lock(configMutex);
		config = std::move(loadedConfig);

		if (fullConfig)
		{
			// Check if config needs migration
			int currentVersion = config.contains("version") ? config["version"].get<int>() : 0;
			if (currentVersion < CURRENT_CONFIG_VERSION)
			{
				migrateConfigUnlocked(currentVersion, CURRENT_CONFIG_VERSION);
			}
			// Ensure all default settings exist
			ensureDefaultConfigUnlocked();
		}

		return Result::Success();
	}
	catch (const std::exception &e)
	{
		defaultConfig();
		return Result::Failure("Error loading configuration: " + std::string(e.what()) + ". Using default values.");
	}
}

void ConfigManager::save(const std::string &path)
{
	json configSnapshot;
	{
		std::lock_guard<std::mutex> lock(configMutex);

		// Ensure version is always present before saving
		if (!config.contains("version"))
		{
			config["version"] = CURRENT_CONFIG_VERSION;
		}

		configSnapshot = config;
	}

	// Enqueue async save
	enqueueSave(path, std::move(configSnapshot));
}

PreferencesSettings ConfigManager::getPreferencesSettings() const
{
	PreferencesSettings settings;
	std::lock_guard<std::mutex> lock(configMutex);
	const json empty = json::object();
	const json &root = config.contains("settings") && config["settings"].is_object() ? config["settings"] : empty;
	const json &speed = root.contains("speed_limits") && root["speed_limits"].is_object() ? root["speed_limits"] : empty;
	const json &search = root.contains("search") && root["search"].is_object() ? root["search"] : empty;
	const json &proxy = root.contains("proxy") && root["proxy"].is_object() ? root["proxy"] : empty;
	settings.downloadSpeedLimit = std::max(speed.value("download", 0), 0);
	settings.uploadSpeedLimit = std::max(speed.value("upload", 0), 0);
	if (config.contains("theme") && config["theme"].is_number_integer())
		settings.theme = config["theme"].get<int>();
	settings.downloadPath = root.value("download_path", "~/Downloads");
	settings.enableDht = root.value("enable_dht", true);
	settings.enableUpnp = root.value("enable_upnp", true);
	settings.enableNatPmp = root.value("enable_natpmp", true);
	settings.torznabEnabled = search.value("torznab_enabled", false);
	settings.torznabUrl = search.value("torznab_url", "");
	settings.proxyEnabled = proxy.value("enabled", false);
	settings.proxyType = proxy.value("type", "socks5");
	settings.proxyHost = proxy.value("host", "127.0.0.1");
	settings.proxyPort = std::clamp(proxy.value("port", 1080), 1, 65535);
	settings.proxyUsername = proxy.value("username", "");
	const json &ui = config.contains("ui") && config["ui"].is_object() ? config["ui"] : empty;
	settings.ui.sidebarWidth = std::clamp(ui.value("sidebar_width", 240), 120, 600);
	settings.ui.bottomPanelHeight = std::clamp(ui.value("bottom_panel_height", 300), 120, 1000);
	settings.ui.sidebarCollapsed = ui.value("sidebar_collapsed", false);
	settings.ui.selectedMainTab = std::max(ui.value("selected_main_tab", 0), 0);
	settings.ui.selectedDetailsTab = std::max(ui.value("selected_details_tab", 0), 0);
	return settings;
}

SaveHandle ConfigManager::savePreferencesCandidate(const std::string &path, const PreferencesSettings &settings)
{
	json candidate;
	{
		std::lock_guard<std::mutex> lock(configMutex);
		candidate = config;
	}
	applyPreferencesToJson(candidate, settings);
	return enqueueSave(path, std::move(candidate));
}

Result ConfigManager::commitPreferences(const PreferencesSettings &settings)
{
	std::lock_guard<std::mutex> lock(configMutex);
	applyPreferencesToJson(config, settings);
	return Result::Success();
}

void ConfigManager::saveTorrents(const std::vector<PersistedTorrent> &torrents)
{
	json torrentsJson;
	std::unordered_map<std::string, std::string> previousResumeData;
	{
		std::lock_guard<std::mutex> lock(configMutex);
		if (config.contains("torrents") && config["torrents"].is_array())
		{
			for (const auto &entry : config["torrents"])
			{
				if (entry.is_object() && entry.value("magnet_uri", "").size() > 0 && entry.contains("resume_data") && entry["resume_data"].is_string())
					previousResumeData.emplace(entry["magnet_uri"].get<std::string>(), entry["resume_data"].get<std::string>());
			}
		}
	}
	for (const auto &torrent : torrents)
	{
		json torrentEntry = {
			{"magnet_uri", torrent.magnetUri},
			{"save_path", torrent.savePath}};

		if (!torrent.torrentFilePath.empty())
			torrentEntry["torrent_path"] = torrent.torrentFilePath;
		if (!torrent.resumeData.empty())
			torrentEntry["resume_data"] = encodeHex(torrent.resumeData);
		else if (auto previous = previousResumeData.find(torrent.magnetUri); previous != previousResumeData.end())
			torrentEntry["resume_data"] = previous->second;

		torrentsJson.push_back(torrentEntry);
	}
	json torrentsFile;
	{
		std::lock_guard<std::mutex> lock(configMutex);
		config["torrents"] = torrentsJson;
		torrentsFile = {{"version", 2}, {"torrents", config["torrents"]}};
	}
	enqueueSave(Utils::AppPaths::torrentsConfigPath().string(), std::move(torrentsFile));
}

Result ConfigManager::loadTorrents(const std::string &path, std::vector<TorrentConfigData> &outTorrents)
{
	outTorrents.clear();

	json sourceConfig;
	bool fileFound = false;
	bool loadedFromFile = false;
	std::string loadError;
	std::vector<std::filesystem::path> candidates;
	if (!path.empty())
	{
		candidates.emplace_back(path);
		candidates.emplace_back(std::filesystem::path(path).string() + ".bak");
	}

	for (const auto &candidate : candidates)
	{
		std::ifstream file(candidate);
		if (!file.is_open())
			continue;
		fileFound = true;
		try
		{
			json candidateConfig;
			file >> candidateConfig;
			if (!candidateConfig.is_object())
			{
				loadError = "Invalid torrents configuration: root must be an object";
				continue;
			}
			if (candidateConfig.contains("torrents") && !candidateConfig["torrents"].is_array())
			{
				loadError = "Invalid torrents configuration: 'torrents' must be an array";
				continue;
			}

			sourceConfig = std::move(candidateConfig);
			loadedFromFile = true;
			if (candidate.extension() == ".bak")
				Utils::Logger::warning("config", "Loading backup torrents configuration: " + candidate.string());
			break;
		}
		catch (const json::parse_error &e)
		{
			loadError = "Failed to parse torrents configuration: " + std::string(e.what());
		}
		catch (const std::exception &e)
		{
			loadError = "Error loading torrents configuration: " + std::string(e.what());
		}
	}

	if (!loadedFromFile)
	{
		if (fileFound)
			return Result::Failure(loadError.empty() ? "Unable to load torrents configuration" : loadError);

		std::lock_guard<std::mutex> lock(configMutex);
		sourceConfig = config;
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
			if (!torrent.is_object())
			{
				Utils::Logger::warning("config", "Skipping a non-object torrent entry");
				continue;
			}
			TorrentConfigData data;
			if (torrent.contains("magnet_uri") && torrent["magnet_uri"].is_string())
				data.magnetUri = torrent["magnet_uri"];
			if (torrent.contains("save_path") && torrent["save_path"].is_string())
				data.savePath = torrent["save_path"];
			if (torrent.contains("torrent_path") && torrent["torrent_path"].is_string())
				data.torrentFilePath = torrent["torrent_path"];
			if (torrent.contains("resume_data") && torrent["resume_data"].is_string())
			{
				if (!decodeHex(torrent["resume_data"].get<std::string>(), data.resumeData))
					Utils::Logger::warning("config", "Ignoring invalid or oversized fast-resume data");
			}

			if (data.savePath.empty() || (data.magnetUri.empty() && data.torrentFilePath.empty() && data.resumeData.empty()))
			{
				Utils::Logger::warning("config", "Skipping an incomplete torrent entry");
				continue;
			}
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

json ConfigManager::getConfig() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	return config;
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
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	if (!config["settings"].contains("speed_limits"))
	{
		config["settings"]["speed_limits"] = json::object();
	}
	config["settings"]["speed_limits"]["download"] = std::max(bytesPerSecond, 0);
}

void ConfigManager::setUploadSpeedLimit(int bytesPerSecond)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	if (!config["settings"].contains("speed_limits"))
	{
		config["settings"]["speed_limits"] = json::object();
	}
	config["settings"]["speed_limits"]["upload"] = std::max(bytesPerSecond, 0);
}

int ConfigManager::getDownloadSpeedLimit() const
{
	std::lock_guard<std::mutex> lock(configMutex);
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
	std::lock_guard<std::mutex> lock(configMutex);
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
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["download_path"] = path;
}

std::string ConfigManager::getDownloadPath() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("settings") && config["settings"].contains("download_path"))
	{
		return config["settings"]["download_path"];
	}
	return "~/Downloads";
}

void ConfigManager::setEnableDHT(bool enable)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["enable_dht"] = enable;
}

bool ConfigManager::getEnableDHT() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("settings") && config["settings"].contains("enable_dht"))
	{
		return config["settings"]["enable_dht"];
	}
	return true; // Default to enabled
}

void ConfigManager::setEnableUPnP(bool enable)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["enable_upnp"] = enable;
}

bool ConfigManager::getEnableUPnP() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("settings") && config["settings"].contains("enable_upnp"))
	{
		return config["settings"]["enable_upnp"];
	}
	return true; // Default to enabled
}

void ConfigManager::setEnableNATPMP(bool enable)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["enable_natpmp"] = enable;
}

bool ConfigManager::getEnableNATPMP() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("settings") && config["settings"].contains("enable_natpmp"))
	{
		return config["settings"]["enable_natpmp"];
	}
	return true; // Default to enabled
}

void ConfigManager::setTorznabUrl(const std::string &url)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["search"]["torznab_url"] = url;
}

std::string ConfigManager::getTorznabUrl() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("settings") && config["settings"].contains("search"))
		return config["settings"]["search"].value("torznab_url", "");
	return {};
}

void ConfigManager::setTorznabEnabled(bool enable)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["search"]["torznab_enabled"] = enable;
}

bool ConfigManager::getTorznabEnabled() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("settings") && config["settings"].contains("search"))
		return config["settings"]["search"].value("torznab_enabled", false);
	return false;
}

void ConfigManager::setProxyEnabled(bool enable)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["proxy"]["enabled"] = enable;
}

bool ConfigManager::getProxyEnabled() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	return config.contains("settings") && config["settings"].contains("proxy")
		? config["settings"]["proxy"].value("enabled", false) : false;
}

void ConfigManager::setProxyType(const std::string &type)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["proxy"]["type"] = type == "http" ? "http" : "socks5";
}

std::string ConfigManager::getProxyType() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	return config.contains("settings") && config["settings"].contains("proxy")
		? config["settings"]["proxy"].value("type", "socks5") : "socks5";
}

void ConfigManager::setProxyHost(const std::string &host)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["proxy"]["host"] = host;
}

std::string ConfigManager::getProxyHost() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	return config.contains("settings") && config["settings"].contains("proxy")
		? config["settings"]["proxy"].value("host", "127.0.0.1") : "127.0.0.1";
}

void ConfigManager::setProxyPort(int port)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["proxy"]["port"] = std::clamp(port, 1, 65535);
}

int ConfigManager::getProxyPort() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (!config.contains("settings") || !config["settings"].contains("proxy"))
		return 1080;
	return std::clamp(config["settings"]["proxy"].value("port", 1080), 1, 65535);
}

void ConfigManager::setProxyUsername(const std::string &username)
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureSettingsStructure();
	config["settings"]["proxy"]["username"] = username;
}

std::string ConfigManager::getProxyUsername() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	return config.contains("settings") && config["settings"].contains("proxy")
		? config["settings"]["proxy"].value("username", "") : "";
}

int ConfigManager::getConfigVersion() const
{
	std::lock_guard<std::mutex> lock(configMutex);
	if (config.contains("version"))
	{
		return config["version"];
	}
	return 0; // Version 0 means unversioned/legacy config
}

void ConfigManager::ensureDefaultConfig()
{
	std::lock_guard<std::mutex> lock(configMutex);
	ensureDefaultConfigUnlocked();
}

void ConfigManager::ensureDefaultConfigUnlocked()
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

	// Fill nested defaults without replacing valid user-owned values or unknown keys.
	applyMissingDefaults(config["settings"], defaults["settings"]);
	if (!config.contains("ui") || !config["ui"].is_object())
		config["ui"] = json::object();
	applyMissingDefaults(config["ui"], defaults["ui"]);
}

void ConfigManager::migrateConfig(int fromVersion, int toVersion)
{
	std::lock_guard<std::mutex> lock(configMutex);
	migrateConfigUnlocked(fromVersion, toVersion);
}

void ConfigManager::migrateConfigUnlocked(int fromVersion, int toVersion)
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

	if (fromVersion <= 1 && toVersion >= 2)
	{
		if (!config.contains("ui") || !config["ui"].is_object())
			config["ui"] = json::object();
		config["version"] = 2;
	}
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
	json configSnapshot;
	{
		std::lock_guard<std::mutex> lock(configMutex);
		config["favorites"] = std::move(favoritesJson);

		// Save search history
		config["search_history"] = searchHistory;

		// Ensure version is always present before saving
		if (!config.contains("version"))
		{
			config["version"] = CURRENT_CONFIG_VERSION;
		}
		configSnapshot = config;
	}

	// Save to file
	enqueueSave(Utils::AppPaths::settingsConfigPath().string(), std::move(configSnapshot));
}

void ConfigManager::loadFavoritesAndHistory(std::vector<TorrentSearchResult> &favorites, std::vector<std::string> &searchHistory)
{
	favorites.clear();
	searchHistory.clear();

	json configSnapshot;
	{
		std::lock_guard<std::mutex> lock(configMutex);
		configSnapshot = config;
	}

	// Load favorites
	if (configSnapshot.contains("favorites") && configSnapshot["favorites"].is_array())
	{
		for (const auto &favJson : configSnapshot["favorites"])
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
	if (configSnapshot.contains("search_history") && configSnapshot["search_history"].is_array())
	{
		for (const auto &historyItem : configSnapshot["search_history"])
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
	std::lock_guard<std::mutex> lock(configMutex);
	config["theme"] = themeIndex;
}

int ConfigManager::getTheme() const
{
	std::lock_guard<std::mutex> lock(configMutex);
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

void ConfigManager::waitForAsyncOperations()
{
	std::unique_lock<std::mutex> lock(queueMutex);
	queueCv.wait(lock, [this] { return activeJobs == 0 && saveQueue.empty(); });
}

void ConfigManager::applyDefaultConfig()
{
	std::lock_guard<std::mutex> lock(configMutex);

	// Use platform-appropriate default download path
	// Note: This is just a fallback default. The actual download path
	// is typically managed by the application preferences controller.
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

	if (config.contains("ui"))
	{
		const auto &ui = config["ui"];
		if (!ui.is_object())
			return false;
		if (ui.contains("sidebar_width") &&
			(!ui["sidebar_width"].is_number_integer() || ui["sidebar_width"].get<int>() < 120 || ui["sidebar_width"].get<int>() > 600))
			return false;
		if (ui.contains("bottom_panel_height") &&
			(!ui["bottom_panel_height"].is_number_integer() || ui["bottom_panel_height"].get<int>() < 120 || ui["bottom_panel_height"].get<int>() > 1000))
			return false;
		if (ui.contains("sidebar_collapsed") && !ui["sidebar_collapsed"].is_boolean())
			return false;
		if (ui.contains("selected_main_tab") &&
			(!ui["selected_main_tab"].is_number_integer() || ui["selected_main_tab"].get<int>() < 0))
			return false;
		if (ui.contains("selected_details_tab") &&
			(!ui["selected_details_tab"].is_number_integer() || ui["selected_details_tab"].get<int>() < 0))
			return false;
	}

	return true;
}

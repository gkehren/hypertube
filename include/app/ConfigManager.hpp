#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <json.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "TorrentManager.hpp"
#include "Result.hpp"

using json = nlohmann::json;

// Forward declarations
struct TorrentSearchResult;

struct TorrentConfigData
{
	std::string magnetUri;
	std::string savePath;
	std::string torrentFilePath;
};

class ConfigManager
{
public:
	ConfigManager();
	~ConfigManager();

	Result load(const std::string &path, bool fullConfig = true);
	void save(const std::string &path);

	void saveTorrents(const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &torrents, const std::unordered_map<lt::sha1_hash, std::string> &torrentFilePaths);
	Result loadTorrents(const std::string &path, std::vector<TorrentConfigData> &outTorrents);

	// Favorites and search history
	void saveFavoritesAndHistory(const std::vector<TorrentSearchResult> &favorites, const std::vector<std::string> &searchHistory);
	void loadFavoritesAndHistory(std::vector<TorrentSearchResult> &favorites, std::vector<std::string> &searchHistory);

	// Speed limit configuration
	void setDownloadSpeedLimit(int bytesPerSecond);
	void setUploadSpeedLimit(int bytesPerSecond);
	int getDownloadSpeedLimit() const;
	int getUploadSpeedLimit() const;

	// New settings configuration
	void setDownloadPath(const std::string &path);
	std::string getDownloadPath() const;
	void setEnableDHT(bool enable);
	bool getEnableDHT() const;
	void setEnableUPnP(bool enable);
	bool getEnableUPnP() const;
	void setEnableNATPMP(bool enable);
	bool getEnableNATPMP() const;

	// Schema management
	int getConfigVersion() const;
	void ensureDefaultConfig();
	void migrateConfig(int fromVersion, int toVersion);
	// Theme configuration
	void setTheme(int themeIndex);
	int getTheme() const;

	// Synchronization for testing
	void waitForAsyncOperations();

	json &getConfig();

private:
	json config;
	static constexpr int CURRENT_CONFIG_VERSION = 1;

	// Async save worker
	struct SaveRequest {
		std::string path;
		json data;
	};

	std::thread saveThread;
	std::queue<SaveRequest> saveQueue;
	std::mutex queueMutex;
	std::condition_variable queueCv;
	std::atomic<bool> stopWorker{false};
	std::atomic<int> activeJobs{0};

	void workerLoop();
	void enqueueSave(const std::string& path, json data);

	json createDefaultConfig() const;
	void ensureSettingsStructure();
	void applyDefaultConfig();
	bool validateConfig();
};

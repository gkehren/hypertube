#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <future>
#include "PersistedTorrent.hpp"
#include "Result.hpp"

using json = nlohmann::json;

// Forward declarations
struct TorrentSearchResult;

struct TorrentConfigData
{
	std::string magnetUri;
	std::string savePath;
	std::string torrentFilePath;
	std::vector<char> resumeData;
};

struct PreferencesSettings
{
	int downloadSpeedLimit = 0;
	int uploadSpeedLimit = 0;
	int theme = 0;
	std::string downloadPath;
	bool enableDht = true;
	bool enableUpnp = true;
	bool enableNatPmp = true;
	bool torznabEnabled = false;
	std::string torznabUrl;
	bool proxyEnabled = false;
	std::string proxyType = "socks5";
	std::string proxyHost;
	int proxyPort = 1080;
	std::string proxyUsername;
	struct UiLayout
	{
		int sidebarWidth = 240;
		int bottomPanelHeight = 300;
		bool sidebarCollapsed = false;
		int selectedMainTab = 0;
		int selectedDetailsTab = 0;
	} ui;
};

using SaveHandle = std::shared_future<Result>;

class ConfigManager
{
public:
	static constexpr int CURRENT_CONFIG_VERSION = 2;

	ConfigManager();
	~ConfigManager();

	Result load(const std::string &path, bool fullConfig = true);
	void save(const std::string &path);
	SaveHandle savePreferencesCandidate(const std::string &path, const PreferencesSettings &settings);
	Result commitPreferences(const PreferencesSettings &settings);
	PreferencesSettings getPreferencesSettings() const;

	void saveTorrents(const std::vector<PersistedTorrent> &torrents);
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
	void setTorznabUrl(const std::string &url);
	std::string getTorznabUrl() const;
	void setTorznabEnabled(bool enable);
	bool getTorznabEnabled() const;
	void setProxyEnabled(bool enable);
	bool getProxyEnabled() const;
	void setProxyType(const std::string &type);
	std::string getProxyType() const;
	void setProxyHost(const std::string &host);
	std::string getProxyHost() const;
	void setProxyPort(int port);
	int getProxyPort() const;
	void setProxyUsername(const std::string &username);
	std::string getProxyUsername() const;

	// Schema management
	int getConfigVersion() const;
	void ensureDefaultConfig();
	void migrateConfig(int fromVersion, int toVersion);
	// Theme configuration
	void setTheme(int themeIndex);
	int getTheme() const;

	// Synchronization for testing
	void waitForAsyncOperations();

	json getConfig() const;

private:
	mutable std::mutex configMutex;
	json config;
	// Async save worker
	struct SaveRequest {
		std::string path;
		json data;
		std::shared_ptr<std::promise<Result>> completion;
	};

	std::thread saveThread;
	std::queue<SaveRequest> saveQueue;
	std::mutex queueMutex;
	std::condition_variable queueCv;
	std::atomic<bool> stopWorker{false};
	std::atomic<int> activeJobs{0};

	void workerLoop();
	SaveHandle enqueueSave(const std::string& path, json data);

	json createDefaultConfig() const;
	void ensureSettingsStructure();
	void ensureDefaultConfigUnlocked();
	void migrateConfigUnlocked(int fromVersion, int toVersion);
	void applyDefaultConfig();
	bool validateConfig();
};

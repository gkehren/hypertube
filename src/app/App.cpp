#include "App.hpp"
#include "AppPaths.hpp"
#include "CredentialStore.hpp"
#include "Logger.hpp"
#include <iostream>
#include <cstdlib>

App::App() = default;

void App::initialize()
{
	if (initialized_)
		return;

	Utils::AppPaths::ensureDirectories();
	Utils::Logger::initialize(Utils::AppPaths::logFilePath());
	Utils::Logger::info("app", "Starting Hypertube");
	initialized_ = true;

	// Load settings before restoring torrents so session behavior is effective
	// from the first network operation.
	const auto torrentsConfigPath = Utils::AppPaths::torrentsConfigPath();
	const auto settingsConfigPath = Utils::AppPaths::settingsConfigPath();
	Result settingsLoadResult = settingsConfigManager_.load(settingsConfigPath.string());
	if (!settingsLoadResult)
		std::cerr << "Warning: " << settingsLoadResult.message << std::endl;
	torrentManager_.setDownloadSpeedLimit(settingsConfigManager_.getDownloadSpeedLimit());
	torrentManager_.setUploadSpeedLimit(settingsConfigManager_.getUploadSpeedLimit());
	torrentManager_.configureDiscovery(
		settingsConfigManager_.getEnableDHT(),
		settingsConfigManager_.getEnableUPnP(),
		settingsConfigManager_.getEnableNATPMP());
	const std::optional<std::string> storedProxyPassword = Utils::CredentialStore::load("proxy_password");
	const bool proxyEnabled = settingsConfigManager_.getProxyEnabled();
	const std::string proxyType = settingsConfigManager_.getProxyType();
	const std::string proxyHost = settingsConfigManager_.getProxyHost();
	const int proxyPort = settingsConfigManager_.getProxyPort();
	const std::string proxyUsername = settingsConfigManager_.getProxyUsername();
	const std::string proxyPassword = storedProxyPassword.value_or("");
	torrentManager_.setProxyConfig(proxyHost, proxyPort, proxyUsername, proxyPassword,
		proxyEnabled ? (proxyType == "http" ? 2 : 1) : 0);
	Result searchProxyResult = searchEngine_.setProxyConfig(
		proxyEnabled, proxyType, proxyHost, proxyPort, proxyUsername, proxyPassword);
	if (!searchProxyResult)
		Utils::Logger::warning("search", "Proxy configuration was ignored: " + searchProxyResult.message);
	if (settingsConfigManager_.getTorznabEnabled())
	{
		std::optional<std::string> storedApiKey = Utils::CredentialStore::load("torznab_api_key");
		const char *environmentApiKey = std::getenv("HYPERTUBE_TORZNAB_API_KEY");
		const std::string apiKey = storedApiKey.value_or(environmentApiKey ? environmentApiKey : "");
		Result providerResult = searchEngine_.configureTorznabProvider(
			settingsConfigManager_.getTorznabUrl(), apiKey);
		if (providerResult)
			searchEngine_.setActiveSearchProvider("torznab");
		else
			Utils::Logger::warning("search", "Torznab configuration was ignored: " + providerResult.message);
	}

	// Load torrents configuration
	Result configLoadResult = torrentsConfigManager_.load(torrentsConfigPath.string(), false);
	if (!configLoadResult)
	{
		std::cerr << "Warning: " << configLoadResult.message << std::endl;
	}

	// Load torrents from config
	std::vector<TorrentConfigData> torrents;
	Result torrentsLoadResult = torrentsConfigManager_.loadTorrents(torrentsConfigPath.string(), torrents);
	if (torrentsLoadResult)
	{
		torrentManager_.addTorrentsFromConfig(torrents);
	}
	else
	{
		std::cerr << "Warning: " << torrentsLoadResult.message << std::endl;
	}

	// Load favorites and search history
	searchEngine_.loadFavoritesAndHistory(settingsConfigManager_);
}

App::~App()
{
	shutdown();
}

void App::shutdown()
{
	if (!initialized_)
		return;
	initialized_ = false;

	// Ensure no search worker can outlive the UI objects it was initiated from.
	searchEngine_.shutdown();
	std::vector<ManagedTorrent> persistenceSnapshot;
	Result resumeResult = torrentManager_.getPersistenceSnapshot(persistenceSnapshot);
	if (!resumeResult)
		Utils::Logger::warning("torrent", resumeResult.message);
	torrentsConfigManager_.saveTorrents(torrentManager_.toPersistedTorrents(persistenceSnapshot));

	// Save favorites and search history
	searchEngine_.saveFavoritesAndHistory(settingsConfigManager_);

	// Wait for background save worker threads to finish writing files
	torrentsConfigManager_.waitForAsyncOperations();
	settingsConfigManager_.waitForAsyncOperations();
	Utils::Logger::info("app", "Shutting down Hypertube");
}

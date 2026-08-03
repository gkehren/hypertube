#include "App.hpp"
#include "AppPaths.hpp"
#include "CredentialStore.hpp"
#include "Logger.hpp"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <cstdlib>

static void glfw_error_callback(int error, const char *description)
{
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

App::App() : uiManager(torrentManager, searchEngine, settingsConfigManager)
{
	Utils::AppPaths::ensureDirectories();
	Utils::Logger::initialize(Utils::AppPaths::logFilePath());
	Utils::Logger::info("app", "Starting Hypertube");
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		throw std::runtime_error("Failed to initialize GLFW");

	// Set OpenGL version to 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow(1440, 720, "Hypertube", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Load settings before restoring torrents so session behavior is effective
	// from the first network operation.
	const auto torrentsConfigPath = Utils::AppPaths::torrentsConfigPath();
	const auto settingsConfigPath = Utils::AppPaths::settingsConfigPath();
	Result settingsLoadResult = settingsConfigManager.load(settingsConfigPath.string());
	if (!settingsLoadResult)
		std::cerr << "Warning: " << settingsLoadResult.message << std::endl;
	torrentManager.setDownloadSpeedLimit(settingsConfigManager.getDownloadSpeedLimit());
	torrentManager.setUploadSpeedLimit(settingsConfigManager.getUploadSpeedLimit());
	torrentManager.configureDiscovery(
		settingsConfigManager.getEnableDHT(),
		settingsConfigManager.getEnableUPnP(),
		settingsConfigManager.getEnableNATPMP());
	const std::optional<std::string> storedProxyPassword = Utils::CredentialStore::load("proxy_password");
	const bool proxyEnabled = settingsConfigManager.getProxyEnabled();
	const std::string proxyType = settingsConfigManager.getProxyType();
	const std::string proxyHost = settingsConfigManager.getProxyHost();
	const int proxyPort = settingsConfigManager.getProxyPort();
	const std::string proxyUsername = settingsConfigManager.getProxyUsername();
	const std::string proxyPassword = storedProxyPassword.value_or("");
	torrentManager.setProxyConfig(proxyHost, proxyPort, proxyUsername, proxyPassword,
		proxyEnabled ? (proxyType == "http" ? 2 : 1) : 0);
	Result searchProxyResult = searchEngine.setProxyConfig(
		proxyEnabled, proxyType, proxyHost, proxyPort, proxyUsername, proxyPassword);
	if (!searchProxyResult)
		Utils::Logger::warning("search", "Proxy configuration was ignored: " + searchProxyResult.message);
	if (settingsConfigManager.getTorznabEnabled())
	{
		std::optional<std::string> storedApiKey = Utils::CredentialStore::load("torznab_api_key");
		const char *environmentApiKey = std::getenv("HYPERTUBE_TORZNAB_API_KEY");
		const std::string apiKey = storedApiKey.value_or(environmentApiKey ? environmentApiKey : "");
		Result providerResult = searchEngine.configureTorznabProvider(
			settingsConfigManager.getTorznabUrl(), apiKey);
		if (providerResult)
			searchEngine.setActiveSearchProvider("torznab");
		else
			Utils::Logger::warning("search", "Torznab configuration was ignored: " + providerResult.message);
	}

	// Load torrents configuration
	Result configLoadResult = torrentsConfigManager.load(torrentsConfigPath.string(), false);
	if (!configLoadResult)
	{
		std::cerr << "Warning: " << configLoadResult.message << std::endl;
	}

	// Load torrents from config
	std::vector<TorrentConfigData> torrents;
	Result torrentsLoadResult = torrentsConfigManager.loadTorrents(torrentsConfigPath.string(), torrents);
	if (torrentsLoadResult)
	{
		torrentManager.addTorrentsFromConfig(torrents);
	}
	else
	{
		std::cerr << "Warning: " << torrentsLoadResult.message << std::endl;
	}

	// Load favorites and search history
	searchEngine.loadFavoritesAndHistory(settingsConfigManager);
}

App::~App()
{
	// Ensure no search worker can outlive the UI objects it was initiated from.
	searchEngine.shutdown();
	std::vector<ManagedTorrent> persistenceSnapshot;
	Result resumeResult = torrentManager.getPersistenceSnapshot(persistenceSnapshot);
	if (!resumeResult)
		Utils::Logger::warning("torrent", resumeResult.message);
	torrentsConfigManager.saveTorrents(persistenceSnapshot);

	// Save favorites and search history
	searchEngine.saveFavoritesAndHistory(settingsConfigManager);

	// Wait for background save worker threads to finish writing files
	torrentsConfigManager.waitForAsyncOperations();
	settingsConfigManager.waitForAsyncOperations();
	Utils::Logger::info("app", "Shutting down Hypertube");

	glfwDestroyWindow(window);
	glfwTerminate();
}

void App::run()
{
	uiManager.init(window);

	static const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	auto nextAutosave = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	while (!glfwWindowShouldClose(window) && !uiManager.shouldExit())
	{
		glfwPollEvents();
		uiManager.renderFrame(window, clear_color);
		glfwSwapBuffers(window);
		if (std::chrono::steady_clock::now() >= nextAutosave)
		{
			torrentsConfigManager.saveTorrents(torrentManager.getTorrentSnapshot());
			searchEngine.saveFavoritesAndHistory(settingsConfigManager);
			settingsConfigManager.save(Utils::AppPaths::settingsConfigPath().string());
			nextAutosave = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		}
	}
	uiManager.shutdown();
}

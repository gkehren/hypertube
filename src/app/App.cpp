#include "App.hpp"
#include "AppPaths.hpp"
#include "Logger.hpp"
#include <stdexcept>
#include <iostream>

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

	// Load torrents configuration
	const auto torrentsConfigPath = Utils::AppPaths::torrentsConfigPath();
	const auto settingsConfigPath = Utils::AppPaths::settingsConfigPath();
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
	Result settingsLoadResult = settingsConfigManager.load(settingsConfigPath.string());
	if (!settingsLoadResult)
	{
		std::cerr << "Warning: " << settingsLoadResult.message << std::endl;
	}
	searchEngine.loadFavoritesAndHistory(settingsConfigManager);
}

App::~App()
{
	torrentsConfigManager.saveTorrents(torrentManager.getTorrentSnapshot());

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
	while (!glfwWindowShouldClose(window) && !uiManager.shouldExit())
	{
		glfwPollEvents();
		uiManager.renderFrame(window, clear_color);
		glfwSwapBuffers(window);
	}
	uiManager.shutdown();
}

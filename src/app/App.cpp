#include "App.hpp"
#include <stdexcept>
#include <iostream>

static void glfw_error_callback(int error, const char *description)
{
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

App::App() : uiManager(torrentManager, searchEngine, settingsConfigManager)
{
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
		throw std::runtime_error("Failed to create GLFW window");
		glfwTerminate();
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	torrentsConfigManager.load("./config/torrents.json");
	torrentManager.addTorrentsFromConfig(torrentsConfigManager.loadTorrents("./config/torrents.json"));

	// Load favorites and search history
	settingsConfigManager.load("./config/settings.json");
	searchEngine.loadFavoritesAndHistory(settingsConfigManager);
}

App::~App()
{
	torrentsConfigManager.saveTorrents(torrentManager.getTorrents(), torrentManager.getTorrentFilePaths());

	// Save favorites and search history
	searchEngine.saveFavoritesAndHistory(settingsConfigManager);

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

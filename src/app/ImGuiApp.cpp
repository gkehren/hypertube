#include "ImGuiApp.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "AppPaths.hpp"

namespace
{
	void glfwErrorCallback(int error, const char *description)
	{
		std::cerr << "GLFW Error " << error << ": " << description << std::endl;
	}
}

ImGuiApp::ImGuiApp(App &app)
	: app_(app),
	  uiManager_(app.torrentManager(), app.searchEngine(), app.settingsConfigManager())
{
}

ImGuiApp::~ImGuiApp()
{
	if (uiInitialized_)
	{
		uiManager_.shutdown();
		uiInitialized_ = false;
	}
	destroyWindow();
}

void ImGuiApp::run()
{
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit())
		throw std::runtime_error("Failed to initialize GLFW");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window_ = glfwCreateWindow(1440, 720, "Hypertube", nullptr, nullptr);
	if (!window_)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}
	glfwMakeContextCurrent(window_);
	glfwSwapInterval(1);

	try
	{
		uiManager_.init(window_);
		uiInitialized_ = true;

		static const ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		auto nextAutosave = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (!glfwWindowShouldClose(window_) && !uiManager_.shouldExit())
		{
			glfwPollEvents();
			uiManager_.renderFrame(window_, clearColor);
			glfwSwapBuffers(window_);
			if (std::chrono::steady_clock::now() >= nextAutosave)
			{
				app_.torrentsConfigManager().saveTorrents(app_.torrentManager().getTorrentSnapshot());
				app_.searchEngine().saveFavoritesAndHistory(app_.settingsConfigManager());
				app_.settingsConfigManager().save(Utils::AppPaths::settingsConfigPath().string());
				nextAutosave = std::chrono::steady_clock::now() + std::chrono::seconds(30);
			}
		}

		uiManager_.shutdown();
		uiInitialized_ = false;
		destroyWindow();
	}
	catch (...)
	{
		if (uiInitialized_)
		{
			uiManager_.shutdown();
			uiInitialized_ = false;
		}
		destroyWindow();
		throw;
	}
}

void ImGuiApp::destroyWindow()
{
	if (window_)
	{
		glfwDestroyWindow(window_);
		window_ = nullptr;
		glfwTerminate();
	}
}

#pragma once

#include <GLFW/glfw3.h>

#include "App.hpp"
#include "UIManager.hpp"

// Transitional ImGui frontend. App owns services; this wrapper owns the
// GLFW/OpenGL and ImGui lifetime while Slint reaches functional parity.
class ImGuiApp
{
public:
	explicit ImGuiApp(App &app);
	~ImGuiApp();

	ImGuiApp(const ImGuiApp &) = delete;
	ImGuiApp &operator=(const ImGuiApp &) = delete;

	void run();

private:
	App &app_;
	GLFWwindow *window_ = nullptr;
	UIManager uiManager_;
	bool uiInitialized_ = false;

	void destroyWindow();
};

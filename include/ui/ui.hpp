#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

class UI {
	public:
		void	init(GLFWwindow* window);
		void	render();
		void	shutdown();
};

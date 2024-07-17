#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <string>

class UI
{
	public:
		void			init(GLFWwindow* window);
		void			render();
		void			shutdown();
		const ImGuiIO&	getIO() const;
		bool			shouldExit() const;

	private:
		ImGuiIO	io;

		bool	exitRequested = false;

		void	saveLayout(const std::string &configFilePath);
		void	loadLayout();
		void	resetLayout();
};

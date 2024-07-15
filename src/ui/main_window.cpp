#include "main_window.hpp"

MainWindow::MainWindow() {
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}
	// Set OpenGL version to 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow(1280, 720, "Main Window", nullptr, nullptr);
	if (!window) {
		throw std::runtime_error("Failed to create GLFW window");
		glfwTerminate();
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync
}

MainWindow::~MainWindow() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void	MainWindow::show() {
	ui.init(window);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ui.render();
		int	width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	ui.shutdown();
}

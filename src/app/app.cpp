#include "app.hpp"
#include <stdexcept>
#include <iostream>

static void glfw_error_callback(int error, const char* description)
{
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

App::App()
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

	// Initialize UI Callbacks
	ui.setGetTorrentsCallback([this]() -> const std::unordered_map<lt::sha1_hash, lt::torrent_handle>& {
		return this->torrentManager.getTorrents();
	});
	ui.setAddTorrentCallback([this](const std::string& torrentPath) -> Result {
		return this->torrentManager.addTorrent(torrentPath);
	});
	ui.setAddMagnetLinkCallback([this](const std::string& magnetUri) -> Result {
		return this->torrentManager.addMagnetTorrent(magnetUri);
	});
	ui.setRemoveTorrentCallback([this](const lt::sha1_hash hash, RemoveTorrentType removeType) -> Result {
		return this->torrentManager.removeTorrent(hash, removeType);
	});

	configManager.load("./config/torrents.json");
	torrentManager.addTorrentsFromVec(configManager.loadTorrents("./config/torrents.json"));
}

App::~App()
{
	configManager.saveTorrents(torrentManager.getTorrents());
	glfwDestroyWindow(window);
	glfwTerminate();
}

void	App::run()
{
	ui.init(window);

	static const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	while (!glfwWindowShouldClose(window) && !ui.shouldExit())
	{
		glfwPollEvents();
		ui.render();
		int	width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Multi-Viewport support
		if (ui.getIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
		glfwSwapBuffers(window);
	}
	ui.shutdown();
}

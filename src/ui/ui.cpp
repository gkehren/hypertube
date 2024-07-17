#include "ui.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

void	UI::init(GLFWwindow* window)
{
	// Initialize ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
	this->io = io;

	// Setup ImGui style
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup platform/renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void	UI::render()
{
	// Start the ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Menu bar
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Add a torrent..."))
			{
				// TODO: Handle torrent file
			}
			if (ImGui::MenuItem("Add a magnet link..."))
			{
				// TODO: Handle magnet link
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Preferences")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit"))
			{
				this->exitRequested = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Save layout"))
				this->saveLayout("config/layout.ini");
			if (ImGui::BeginMenu("Load layout"))
			{
				this->loadLayout();
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Reset layout"))
				this->resetLayout();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About"))
				ImGui::ShowAboutWindow();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	ImGui::Begin("Log");
	ImGui::Text("Here will be the log.");
	ImGui::End();

	ImGui::Begin("Torrent List");
	ImGui::Text("Here will be the list of torrents.");
	ImGui::End();

	ImGui::Begin("Torrent Details");
	ImGui::Text("Here will be the details of the selected torrent.");
	ImGui::End();

	ImGui::ShowDemoWindow();

	// Render ImGui
	ImGui::Render();
}

void	UI::shutdown()
{
	// Shutdown platform/renderer bindings
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	// Shutdown ImGui context
	ImGui::DestroyContext();
}

const ImGuiIO&	UI::getIO() const
{
	return (io);
}

bool	UI::shouldExit() const
{
	return (exitRequested);
}

void	UI::saveLayout(const std::string &configFilePath)
{
	size_t	size;
	const char*	data = ImGui::SaveIniSettingsToMemory(&size);

	std::ofstream file(configFilePath, std::ios::out | std::ios::binary);
	if (file.is_open())
	{
		file.write(data, size);
		file.close();
	}

	ImGui::MemFree((void*)data);
}

void	UI::loadLayout()
{
	const std::string configPath = "config";
	if (!std::filesystem::exists(configPath))
	{
		std::cerr << "Config folder not found" << std::endl;
		return;
	}

	for (const auto& entry : std::filesystem::directory_iterator(configPath))
	{
		if (entry.path().extension() == ".ini")
		{
			if (ImGui::MenuItem(entry.path().filename().string().c_str()))
			{
				ImGui::LoadIniSettingsFromDisk(entry.path().string().c_str());
			}
		}
	}
}

void	UI::resetLayout()
{
	ImGui::LoadIniSettingsFromDisk("config/default_layout.ini");
}

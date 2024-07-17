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

	bool showMagnetTorrentPopup = false;

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
				showMagnetTorrentPopup = true;
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

	if (showMagnetTorrentPopup)
	{
		ImGui::OpenPopup("AddMangetTorrentPopup");
	}
	addMagnetTorrentModal();

	displayTorrentList();

	ImGui::Begin("Torrent Details");
	ImGui::Text("Here will be the details of the selected torrent.");
	ImGui::End();

	ImGui::ShowDemoWindow();

	// Render ImGui
	ImGui::Render();
}

static std::string	torrentStateToString(lt::torrent_status::state_t state)
{
	switch (state)
	{
		case lt::torrent_status::downloading_metadata:
			return "Downloading metadata";
		case lt::torrent_status::downloading:
			return "Downloading";
		case lt::torrent_status::finished:
			return "Finished";
		case lt::torrent_status::seeding:
			return "Seeding";
		case lt::torrent_status::checking_files:
			return "Checking files";
		case lt::torrent_status::checking_resume_data:
			return "Checking resume data";
		default:
			return "Unknown";
	}
}

void	UI::displayTorrentList()
{
	if (ImGui::Begin("Torrent List"))
	{
		if (ImGui::BeginTable("Torrents", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();

			if (getTorrentsCallback)
			{
				auto torrents = getTorrentsCallback();
				for (const auto& [info_hash, handle] : torrents)
				{
					lt::torrent_status	status = handle.status();
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%s", status.name.c_str());
					ImGui::TableNextColumn();
					ImGui::ProgressBar(status.progress);
					ImGui::TableNextColumn();
					ImGui::Text("%s", torrentStateToString(status.state));
				}
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}
	
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


// Layout Management
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


// Modal Windows
void	UI::addTorrentModal()
{}

void	UI::addMagnetTorrentModal()
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("AddMangetTorrentPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter the magnet link:");
		ImGui::Separator();
		static char buffer[4096];
		ImGui::InputText("##MagnetLink", buffer, IM_ARRAYSIZE(buffer));

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			if (addMagnetLinkCallback) {
				addMagnetLinkCallback(buffer);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

// Callbacks
void	UI::setAddMagnetLinkCallback(std::function<void(const std::string&)> callback)
{
	this->addMagnetLinkCallback = callback;
}

void	UI::setGetTorrentsCallback(std::function<std::unordered_map<lt::sha1_hash, lt::torrent_handle>&()> callback)
{
	this->getTorrentsCallback = callback;
}
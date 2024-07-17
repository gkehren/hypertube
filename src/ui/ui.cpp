#include "ui.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
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

static std::string	formatBytes(int64_t bytes, bool speed)
{
	const char*	units[] = { "B", "KB", "MB", "GB", "TB" };
	int64_t		size = bytes;
	int			unitIndex = 0;

	while (size >= 1024 && unitIndex < sizeof(units) / sizeof(units[0]) - 1)
	{
		size /= 1024;
		unitIndex++;
	}
	
	std::ostringstream	oss;
	oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
	if (speed)
		oss << "/s";
	return oss.str();
}

static std::string	computeETA(lt::torrent_status* status)
{
	if (status->state == lt::torrent_status::downloading && status->download_payload_rate > 0)
	{
		int64_t		secondsLeft = (status->total_wanted - status->total_wanted_done) / status->download_payload_rate;
		int64_t		minutesLeft = secondsLeft / 60;
		int64_t		hoursLeft = minutesLeft / 60;
		int64_t		daysLeft = hoursLeft / 24;

		if (daysLeft > 0)
			return std::to_string(daysLeft) + " days";
		if (hoursLeft > 0)
			return std::to_string(hoursLeft) + " hours";
		if (minutesLeft > 0)
			return std::to_string(minutesLeft) + " minutes";
		return std::to_string(secondsLeft) + " seconds";
	}
	return "N/A";
}

void	UI::displayTorrentList()
{
	if (ImGui::Begin("Torrent List"))
	{
		if (ImGui::BeginTable("Torrents", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Size");
			ImGui::TableSetupColumn("Progress");
			ImGui::TableSetupColumn("Status");
			ImGui::TableSetupColumn("Down Speed");
			ImGui::TableSetupColumn("Up Speed");
			ImGui::TableSetupColumn("ETA");
			ImGui::TableSetupColumn("Seeds/Peers");
			ImGui::TableHeadersRow();

			if (getTorrentsCallback)
			{
				auto& torrents = getTorrentsCallback();
				for (const auto& [info_hash, handle] : torrents)
				{
					lt::torrent_status	status = handle.status();
					ImGui::PushID(&handle);
					ImGui::TableNextRow();
					for (int col = 0; col < 9; col++)
					{
						ImGui::TableSetColumnIndex(col);
						std::string	cell_text;

						switch (col)
						{
                            case 0: cell_text = std::to_string(static_cast<int>(status.queue_position)); break;
							case 1: cell_text = status.name; break;
							case 2: cell_text = formatBytes(status.total_wanted, false); break;
							case 3: ImGui::ProgressBar(status.progress); break;
							case 4: cell_text = torrentStateToString(status.state); break;
							case 5: cell_text = formatBytes(status.download_payload_rate, true); break;
							case 6: cell_text = formatBytes(status.upload_payload_rate, true); break;
							case 7: cell_text = computeETA(&status); break;
							case 8: cell_text = std::to_string(status.num_seeds) + "/" + std::to_string(status.num_peers) + " (" + std::to_string(status.num_seeds / (float)status.num_peers) + ")"; break;
						}
						if (ImGui::Selectable(cell_text.c_str(), this->selectedTorrent == handle, ImGuiSelectableFlags_SpanAllColumns))
						{
							this->selectedTorrent = handle;
						}
					}
					ImGui::PopID();
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
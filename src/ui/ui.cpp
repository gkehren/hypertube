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

	bool	showMagnetTorrentPopup = false;
	bool	showTorrentPopup = false;

	// Menu bar
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Add a torrent...", "CTRL+O"))
			{
				showTorrentPopup = true;
			}
			if (ImGui::MenuItem("Add a magnet link...", "CTRL+U"))
			{
				showMagnetTorrentPopup = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Preferences", "CTRL+P")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "ALT+F4"))
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

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	ImGui::Begin("Log");
	ImGui::Text("Here will be the log.");
	ImGui::End();

	if (showTorrentPopup)
	{
		IGFD::FileDialogConfig	config;
		config.path = ".";
		config.countSelectionMax = 1;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseTorrentFile", "Choose a .torrent file", ".torrent", config);
	}
	if (ImGuiFileDialog::Instance()->Display("ChooseTorrentFile"))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			std::cout << "Selected file: " << filePath << std::endl;
			if (addTorrentCallback)
			{
				Result result = addTorrentCallback(filePath);
				if (!result)
				{
					this->showFailurePopup = true;
					this->failurePopupMessage = result.message;
				}
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if (showMagnetTorrentPopup)
	{
		ImGui::OpenPopup("Add Magnet Torrent");
	}
	addMagnetTorrentModal();

	displayTorrentList();
	displayTorrentDetails();

	if (this->torrentsToRemove.size() > 0)
	{
		ImGui::OpenPopup("Remove Torrent");
	}
	removeTorrentModal();

	if (this->showFailurePopup)
	{
		ImGui::OpenPopup("Failure");
		this->showFailurePopup = false;
	}
	renderPopupFailure(this->failurePopupMessage);

	ImGui::ShowDemoWindow();

	// Render ImGui
	ImGui::Render();
}

static std::string	torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
{
	if (flags & lt::torrent_flags::paused)
		return "Paused";
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

static std::string	formatBytes(size_t bytes, bool speed)
{
	const char*	units[] = { "B", "KB", "MB", "GB", "TB" };
	size_t		size = bytes;
	size_t		unitIndex = 0;

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

static std::string sha1HashToHex(const lt::sha1_hash& hash)
{
	const char* hexChars = "0123456789ABCDEF";
	constexpr std::size_t hashSize = lt::sha1_hash::size();
	std::string hexString(hashSize * 2, ' ');

	for (std::size_t i = 0; i < hashSize; ++i)
	{
		const unsigned char byte = hash[i];
		hexString[i * 2] = hexChars[byte >> 4];
		hexString[i * 2 + 1] = hexChars[byte & 0x0F];
	}

	return hexString;
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
							case 0: cell_text = std::to_string(static_cast<int>(status.queue_position) + 1); break;
							case 1: cell_text = status.name; break;
							case 2: cell_text = formatBytes(status.total_wanted, false); break;
							case 3: ImGui::ProgressBar(status.progress); break;
							case 4: cell_text = torrentStateToString(status.state, handle.flags()); break;
							case 5: cell_text = formatBytes(status.download_payload_rate, true); break;
							case 6: cell_text = formatBytes(status.upload_payload_rate, true); break;
							case 7: cell_text = computeETA(&status); break;
							case 8: cell_text = std::to_string(status.num_seeds) + "/" + std::to_string(status.num_peers) + " (" + std::to_string(status.num_seeds / (float)status.num_peers) + ")"; break;
						}
						if (col != 3)
						{
							if (ImGui::Selectable(cell_text.c_str(), this->selectedTorrent == handle, ImGuiSelectableFlags_SpanAllColumns))
							{
								this->selectedTorrent = handle;
							}
						}
						ImGui::SameLine();
					}
					if (ImGui::BeginPopupContextItem("##context", ImGuiPopupFlags_MouseButtonRight))
					{
						if (ImGui::MenuItem("Open"))
						{}
						if (ImGui::MenuItem("Open Containing Folder"))
						{
							#ifdef _WIN32
							std::string command = "explorer.exe \"" + status.save_path + "\"";
							#elif __APPLE__
							std::string command = "open \"" + status.save_path + "\"";
							#elif __linux__
							std::string command = "xdg-open \"" + status.save_path + "\"";
							#endif
							int ret = system(command.c_str());
							if (ret != 0)
							{
								std::cerr << "Failed to open containing folder" << std::endl;
							}
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Copy Magnet URI"))
						{
							ImGui::SetClipboardText(lt::make_magnet_uri(handle).c_str());
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Force Start"))
						{
							handle.force_recheck();
							handle.resume();
						}
						if (ImGui::MenuItem("Start"))
						{
							handle.resume();
						}
						if (ImGui::MenuItem("Pause"))
						{
							if (handle.flags() & lt::torrent_flags::paused)
								handle.resume();
							else
								handle.pause();
						}
						if (ImGui::MenuItem("Stop"))
						{
							handle.pause();
							handle.force_recheck();
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Move Up Queue"))
						{
							handle.queue_position_up();
						}
						if (ImGui::MenuItem("Move Down Queue"))
						{
							handle.queue_position_down();
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Remove"))
						{
							this->torrentsToRemove.emplace_back(info_hash, REMOVE_TORRENT);
						}
						if (ImGui::BeginMenu("Remove And"))
						{
							if (ImGui::MenuItem("Delete .torrent"))
							{
								this->torrentsToRemove.emplace_back(info_hash, REMOVE_TORRENT_FILES);
							}
							if (ImGui::MenuItem("Delete .torrent + Data"))
							{
								this->torrentsToRemove.emplace_back(info_hash, REMOVE_TORRENT_FILES_AND_DATA);
							}
							if (ImGui::MenuItem("Delete Data"))
							{
								this->torrentsToRemove.emplace_back(info_hash, REMOVE_TORRENT_DATA);
							}
							ImGui::EndMenu();
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Update Tracker"))
						{
							handle.force_reannounce();
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Properties"))
						{}
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}
}

void	UI::displayTorrentDetails()
{
	ImGui::Begin("Torrent Details");
	if (this->selectedTorrent.is_valid())
	{
		lt::torrent_status	status = this->selectedTorrent.status();
		ImGui::Text("Name: %s", status.name.c_str());
		ImGui::Text("Size: %s", formatBytes(status.total_wanted, false).c_str());
		ImGui::Text("Progress: %.2f%%", status.progress * 100);
		ImGui::Text("Status: %s", torrentStateToString(status.state, this->selectedTorrent.flags()).c_str());
		ImGui::Text("Down Speed: %s", formatBytes(status.download_payload_rate, true).c_str());
		ImGui::Text("Up Speed: %s", formatBytes(status.upload_payload_rate, true).c_str());
		ImGui::Text("ETA: %s", computeETA(&status).c_str());
		ImGui::Text("Seeds/Peers: %d/%d (%.2f)", status.num_seeds, status.num_peers, status.num_seeds / (float)status.num_peers);
	}
	else
	{
		ImGui::Text("No torrent selected");
	}
	ImGui::End();
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

	if (ImGui::BeginPopupModal("Add Magnet Torrent", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter the magnet link:");
		ImGui::Separator();
		ImGui::InputText("##MagnetLink", this->magnetLinkBuffer, IM_ARRAYSIZE(this->magnetLinkBuffer));

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			if (addMagnetLinkCallback) {
				Result result = addMagnetLinkCallback(this->magnetLinkBuffer);
				if (!result)
				{
					this->showFailurePopup = true;
					this->failurePopupMessage = result.message;
				}
			}
			memset(this->magnetLinkBuffer, 0, sizeof(this->magnetLinkBuffer));
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

void	UI::renderPopupFailure(const std::string &message)
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Failure", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("%s", message.c_str());
		ImGui::Separator();

		if (ImGui::Button("OK"))
		{
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

void	UI::removeTorrentModal()
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Remove Torrent", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Are you sure you want to remove the selected torrent?");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			for (const auto& removalInfo : this->torrentsToRemove)
			{
				if (removeTorrentCallback)
				{
					Result result = removeTorrentCallback(removalInfo.hash, removalInfo.removeType);
					if (!result)
					{
						this->showFailurePopup = true;
						this->failurePopupMessage = result.message;
					}
				}
			}
			this->torrentsToRemove.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			this->torrentsToRemove.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

// Callbacks
void	UI::setAddTorrentCallback(std::function<Result(const std::string&)> callback)
{
	this->addTorrentCallback = callback;
}

void	UI::setAddMagnetLinkCallback(std::function<Result(const std::string&)> callback)
{
	this->addMagnetLinkCallback = callback;
}

void	UI::setGetTorrentsCallback(std::function<const std::unordered_map<lt::sha1_hash, lt::torrent_handle>&()> callback)
{
	this->getTorrentsCallback = callback;
}

void	UI::setRemoveTorrentCallback(std::function<Result(const lt::sha1_hash, RemoveTorrentType)> callback)
{
	this->removeTorrentCallback = callback;
}

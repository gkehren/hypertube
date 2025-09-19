#include "UIManager.hpp"
#include "imgui_internal.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <cstring>

UIManager::UIManager(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void UIManager::init(GLFWwindow *window)
{
	initImGui(window);
	setDefaultSavePath();
}

void UIManager::setDefaultSavePath()
{
// Set defaultSavePath to downloads directory of the current user
#ifdef _WIN32
	this->defaultSavePath = std::string(std::getenv("USERPROFILE")) + "\\Downloads";
#elif __APPLE__
	this->defaultSavePath = std::string(std::getenv("HOME")) + "/Downloads";
#elif __linux__
	this->defaultSavePath = std::string(std::getenv("HOME")) + "/Downloads";
#endif
}

void UIManager::initImGui(GLFWwindow *window)
{
	// Initialize ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io_ref = ImGui::GetIO();
	io_ref.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io_ref.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io_ref.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io_ref.IniFilename = NULL;
	this->io = io_ref;

	// Setup ImGui style
	ImGui::StyleColorsDark();

	ImGuiStyle &style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup platform/renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void UIManager::renderFrame(GLFWwindow *window, const ImVec4 &clear_color)
{
	// Start the ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Create the docking environment
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	static bool first_time = true;
	if (first_time)
	{
		first_time = false;
		setupDocking(dockspace_id);
	}

	bool showMagnetTorrentPopup = false;
	bool showTorrentPopup = false;

	displayMenuBar(showTorrentPopup, showMagnetTorrentPopup);

	displayCategories();
	displayTorrentList();
	displayTorrentDetails();

	handleAddTorrentModal(showTorrentPopup);
	handleAddMagnetTorrentModal(showMagnetTorrentPopup);
	handleRemoveTorrentModal();
	handleAskSavePathModal();

	if (this->showFailurePopup)
	{
		ImGui::OpenPopup("Failure");
		this->showFailurePopup = false;
	}
	renderPopupFailure(this->failurePopupMessage);

	ImGui::End();

	// Render ImGui
	ImGui::Render();

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);
	glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Multi-Viewport support
	if (this->getIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow *backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}

void UIManager::setupDocking(ImGuiID dockspace_id)
{
	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

	ImGuiID dock_main_id = dockspace_id;
	ImGuiID dock_left_id;
	ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, &dock_left_id, &dock_main_id);

	ImGuiID dock_top_id;
	ImGuiID dock_bottom_id;
	ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.4f, &dock_bottom_id, &dock_top_id);

	ImGui::DockBuilderDockWindow("Categories", dock_left_id);
	ImGui::DockBuilderDockWindow("Torrent List", dock_top_id);
	ImGui::DockBuilderDockWindow("Torrent Details", dock_bottom_id);

	ImGui::DockBuilderFinish(dockspace_id);
}

void UIManager::handleAddTorrentModal(bool &showTorrentPopup)
{
	if (showTorrentPopup)
	{
		IGFD::FileDialogConfig config;
		config.path = defaultSavePath;
		config.countSelectionMax = 1;
		config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowDevicesButton | ImGuiFileDialogFlags_DontShowHiddenFiles;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseTorrentFile", "Choose a .torrent file", ".torrent", config);
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGuiFileDialog::Instance()->Display("ChooseTorrentFile", ImGuiWindowFlags_NoCollapse, ImVec2(800, 600)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			if (!filePath.empty())
			{
				torrentToAdd.first = false;
				torrentToAdd.second = filePath;
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void UIManager::handleAddMagnetTorrentModal(bool &showMagnetTorrentPopup)
{
	if (showMagnetTorrentPopup)
	{
		ImGui::OpenPopup("Add Magnet Torrent");
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Add Magnet Torrent", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter the magnet link:");
		ImGui::Separator();
		ImGui::InputText("##MagnetLink", this->magnetLinkBuffer, IM_ARRAYSIZE(this->magnetLinkBuffer));

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			torrentToAdd.first = true;
			torrentToAdd.second = this->magnetLinkBuffer;
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

void UIManager::handleRemoveTorrentModal()
{
	if (this->torrentsToRemove.size() > 0)
	{
		ImGui::OpenPopup("Remove Torrent");
	}
	removeTorrentModal();
}

void UIManager::handleAskSavePathModal()
{
	if (!this->torrentToAdd.second.empty())
	{
		IGFD::FileDialogConfig config;
		config.path = savePath.empty() ? defaultSavePath : savePath;
		config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowDevicesButton | ImGuiFileDialogFlags_DontShowHiddenFiles;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseSavePath", "Choose a directory to save the torrent", nullptr, config);
	}
	askSavePathModal();
}

void UIManager::displayMenuBar(bool &showTorrentPopup, bool &showMagnetTorrentPopup)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			const std::vector<MenuItem> menuItems = {
				{"Add a torrent...", "CTRL+O", [&]()
				 { showTorrentPopup = true; }},
				{"Add a magnet link...", "CTRL+U", [&]()
				 { showMagnetTorrentPopup = true; }},
				{"Preferences", "CTRL+P", []() {}},
				{"Exit", "ALT+F4", [&]()
				 { this->exitRequested = true; }},
			};

			for (const auto &item : menuItems)
			{
				if (ImGui::MenuItem(item.label.c_str(), item.shortcut.c_str()))
				{
					item.action();
				}
			}
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
}

std::string UIManager::torrentStateToString(lt::torrent_status::state_t state, lt::torrent_flags_t flags)
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

std::string UIManager::formatBytes(size_t bytes, bool speed)
{
	const char *units[] = {"B", "KB", "MB", "GB", "TB"};
	size_t size = bytes;
	size_t unitIndex = 0;

	while (size >= 1024 && unitIndex < sizeof(units) / sizeof(units[0]) - 1)
	{
		size /= 1024;
		unitIndex++;
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
	if (speed)
		oss << "/s";
	return oss.str();
}

std::string UIManager::computeETA(const lt::torrent_status &status) const
{
	if (status.state == lt::torrent_status::downloading && status.download_payload_rate > 0)
	{
		int64_t secondsLeft = (status.total_wanted - status.total_wanted_done) / status.download_payload_rate;
		int64_t minutesLeft = secondsLeft / 60;
		int64_t hoursLeft = minutesLeft / 60;
		int64_t daysLeft = hoursLeft / 24;

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

static std::string sha1HashToHex(const lt::sha1_hash &hash)
{
	const char *hexChars = "0123456789ABCDEF";
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

void UIManager::displayTorrentList()
{
	if (ImGui::Begin("Torrent List"))
	{
		if (ImGui::BeginTable("Torrents", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			displayTorrentTableHeader();
			displayTorrentTableBody();
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void UIManager::displayTorrentTableHeader()
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
}

void UIManager::displayTorrentTableBody()
{
	auto &torrents = torrentManager.getTorrents();
	for (const auto &[info_hash, handle] : torrents)
	{
		displayTorrentTableRow(handle, info_hash);
	}
}

void UIManager::displayTorrentTableRow(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash)
{
	lt::torrent_status status = handle.status();
	ImGui::PushID(&handle);
	ImGui::TableNextRow();
	for (int col = 0; col < 9; col++)
	{
		ImGui::TableSetColumnIndex(col);
		std::string cell_text = getTorrentCellText(status, col, handle);
		if (col != 3)
		{
			if (ImGui::Selectable(cell_text.c_str(), this->selectedTorrent == handle, ImGuiSelectableFlags_SpanAllColumns))
			{
				this->selectedTorrent = handle;
			}
		}
		else
		{
			ImGui::ProgressBar(status.progress);
		}
		ImGui::SameLine();
	}
	displayTorrentContextMenu(handle, info_hash);
	ImGui::PopID();
}

std::string UIManager::getTorrentCellText(const lt::torrent_status &status, int column, const lt::torrent_handle &handle)
{
	switch (column)
	{
	case 0:
		return std::to_string(static_cast<int>(status.queue_position) + 1);
	case 1:
		return status.name;
	case 2:
		return formatBytes(status.total_wanted, false);
	case 4:
		return torrentStateToString(status.state, handle.flags());
	case 5:
		return formatBytes(status.download_payload_rate, true);
	case 6:
		return formatBytes(status.upload_payload_rate, true);
	case 7:
		return computeETA(status);
	case 8:
		return std::to_string(status.num_seeds) + "/" + std::to_string(status.num_peers) + " (" + std::to_string(status.num_seeds / (float)status.num_peers) + ")";
	default:
		return "";
	}
}

void UIManager::displayTorrentContextMenu(const lt::torrent_handle &handle, const lt::sha1_hash &info_hash)
{
	if (ImGui::BeginPopupContextItem("##context", ImGuiPopupFlags_MouseButtonRight))
	{
		const std::vector<MenuItem> menuItems = {
			{"Open", "", []() {}},
			{"Open Containing Folder", "", [=]()
			 {
#ifdef _WIN32
				 std::string command = "explorer.exe \"" + handle.status().save_path + "\"";
#elif __APPLE__
				std::string command = "open \"" + handle.status().save_path + "\"";
#elif __linux__
				std::string command = "xdg-open \"" + handle.status().save_path + "\"";
#endif
				 int ret = system(command.c_str());
				 if (ret != 0)
				 {
					 std::cerr << "Failed to open containing folder" << std::endl;
				 }
			 }},
			{"Copy Magnet URI", "", [=]()
			 { ImGui::SetClipboardText(lt::make_magnet_uri(handle).c_str()); }},
			{"Force Start", "", [=]()
			 { handle.force_recheck(); handle.resume(); }},
			{"Start", "", [=]()
			 { handle.resume(); }},
			{"Pause", "", [=]()
			 { if (handle.flags() & lt::torrent_flags::paused) handle.resume(); else handle.pause(); }},
			{"Stop", "", [=]()
			 { handle.pause(); handle.force_recheck(); }},
			{"Move Up Queue", "", [=]()
			 { handle.queue_position_up(); }},
			{"Move Down Queue", "", [=]()
			 { handle.queue_position_down(); }},
			{"Remove", "", [=]()
			 { this->torrentsToRemove.emplace_back(info_hash, REMOVE_TORRENT); }},
			{"Update Tracker", "", [=]()
			 { handle.force_reannounce(); }},
			{"Properties", "", []() {}},
		};

		for (const auto &item : menuItems)
		{
			if (ImGui::MenuItem(item.label.c_str(), item.shortcut.c_str()))
			{
				item.action();
			}
		}
		ImGui::EndPopup();
	}
}

void UIManager::displayTorrentDetails()
{
	ImGui::Begin("Torrent Details");
	if (this->selectedTorrent.is_valid())
	{
		lt::torrent_status status = this->selectedTorrent.status();
		if (ImGui::BeginTabBar("TorrentDetailsTabBar"))
		{
			if (ImGui::BeginTabItem("General"))
			{
				displayTorrentDetails_General(status);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Files"))
			{
				displayTorrentDetails_Files();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Peers"))
			{
				displayTorrentDetails_Peers();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Trackers"))
			{
				displayTorrentDetails_Trackers();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	else
	{
		ImGui::Text("No torrent selected");
	}
	ImGui::End();
}

void UIManager::displayTorrentDetails_General(const lt::torrent_status &status)
{
	displayTorrentDetailsContent(status);
}

void UIManager::displayTorrentDetails_Files()
{
	if (!selectedTorrent.is_valid())
		return;

	auto file_storage = selectedTorrent.torrent_file()->files();
	std::vector<std::int64_t> file_progress;
	selectedTorrent.file_progress(file_progress);

	if (ImGui::BeginTable("Files", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Size");
		ImGui::TableSetupColumn("Progress");
		ImGui::TableHeadersRow();

		for (int i = 0; i < file_storage.num_files(); ++i)
		{
			lt::file_index_t index(i);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", file_storage.file_name(index).to_string().c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", formatBytes(file_storage.file_size(index), false).c_str());
			ImGui::TableSetColumnIndex(2);
			if (file_storage.file_size(index) > 0)
				ImGui::ProgressBar(static_cast<float>(file_progress[i]) / file_storage.file_size(index));
			else
				ImGui::ProgressBar(0.0f);
		}
		ImGui::EndTable();
	}
}

void UIManager::displayTorrentDetails_Peers()
{
	if (!selectedTorrent.is_valid())
		return;

	std::vector<lt::peer_info> peers;
	selectedTorrent.get_peer_info(peers);

	if (ImGui::BeginTable("Peers", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("IP");
		ImGui::TableSetupColumn("Client");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableSetupColumn("Down Speed");
		ImGui::TableSetupColumn("Up Speed");
		ImGui::TableHeadersRow();

		for (const auto &peer : peers)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", peer.ip.address().to_string().c_str());
			ImGui::TableSetColumnIndex(1);

			// Safely display peer client by limiting length and handling non-printable chars
			std::string client = peer.client;
			if (client.length() > 8)
			{
				client = client.substr(0, 8);
			}
			// Replace any non-printable characters with '.'
			for (char &c : client)
			{
				if (c < 32 || c > 126)
				{
					c = '.';
				}
			}
			ImGui::Text("%s", client.c_str());

			ImGui::TableSetColumnIndex(2);
			// TODO: Display flags
			ImGui::Text("TODO");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%s", formatBytes(peer.payload_down_speed, true).c_str());
			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%s", formatBytes(peer.payload_up_speed, true).c_str());
		}
		ImGui::EndTable();
	}
}

void UIManager::displayTorrentDetails_Trackers()
{
	if (!selectedTorrent.is_valid())
		return;

	auto trackers = selectedTorrent.trackers();

	if (ImGui::BeginTable("Trackers", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("URL");
		ImGui::TableSetupColumn("Status");
		ImGui::TableHeadersRow();

		for (const auto &tracker : trackers)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", tracker.url.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", tracker.verified ? "Verified" : "Not Verified");
		}
		ImGui::EndTable();
	}
}

void UIManager::displayCategories()
{
	ImGui::Begin("Categories");
	ImGui::Text("All");
	ImGui::Text("Downloading");
	ImGui::Text("Seeding");
	ImGui::Text("Completed");
	ImGui::Text("Paused");
	ImGui::Text("Active");
	ImGui::Text("Inactive");
	ImGui::End();
}

void UIManager::displayTorrentDetailsContent(const lt::torrent_status &status)
{
	const std::vector<std::pair<std::string, std::function<std::string()>>> details = {
		{"Name", [&]()
		 { return status.name; }},
		{"Size", [&]()
		 { return this->formatBytes(status.total_wanted, false); }},
		{"Progress", [&]()
		 { return std::to_string(status.progress * 100) + "%%"; }},
		{"Status", [&]()
		 { return this->torrentStateToString(status.state, this->selectedTorrent.flags()); }},
		{"Down Speed", [&]()
		 { return this->formatBytes(status.download_payload_rate, true); }},
		{"Up Speed", [&]()
		 { return this->formatBytes(status.upload_payload_rate, true); }},
		{"ETA", [&]()
		 { return this->computeETA(status); }},
		{"Seeds/Peers", [&]()
		 { return std::to_string(status.num_seeds) + "/" + std::to_string(status.num_peers) + " (" + std::to_string(status.num_seeds / (float)status.num_peers) + ")"; }},
	};

	for (const auto &detail : details)
	{
		ImGui::Text("%s: %s", detail.first.c_str(), detail.second().c_str());
	}
}

const ImGuiIO &UIManager::getIO() const
{
	return io;
}

bool UIManager::shouldExit() const
{
	return exitRequested;
}

void UIManager::shutdown()
{
	// Shutdown platform/renderer bindings
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	// Shutdown ImGui context
	ImGui::DestroyContext();
}

// Modal Windows

void UIManager::askSavePathModal()
{
	if (torrentToAdd.second.empty())
		return;

	if (ImGuiFileDialog::Instance()->Display("ChooseSavePath", ImGuiWindowFlags_NoCollapse, ImVec2(800, 600)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			this->savePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			if (!savePath.empty())
			{
				Result result(true);
				if (torrentToAdd.first)
				{
					result = torrentManager.addMagnetTorrent(torrentToAdd.second, savePath);
				}
				else
				{
					result = torrentManager.addTorrent(torrentToAdd.second, savePath);
				}
				if (!result)
				{
					this->showFailurePopup = true;
					this->failurePopupMessage = result.message;
				}
				torrentToAdd.second.clear();
			}
			ImGuiFileDialog::Instance()->Close();
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void UIManager::renderPopupFailure(const std::string &message)
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

void UIManager::removeTorrentModal()
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Remove Torrent", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Are you sure you want to remove the selected torrent?");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			for (const auto &removalInfo : this->torrentsToRemove)
			{
				Result result = torrentManager.removeTorrent(removalInfo.hash, removalInfo.removeType);
				if (!result)
				{
					this->showFailurePopup = true;
					this->failurePopupMessage = result.message;
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
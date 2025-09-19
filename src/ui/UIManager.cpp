#include "UIManager.hpp"
#include "imgui_internal.h"
#include <filesystem>
#include <iostream>
#include <memory>

UIManager::UIManager(TorrentManager &torrentManager, SearchEngine &searchEngine)
	: torrentManager(torrentManager), searchEngine(searchEngine)
{
	// Initialize UI components
	torrentTableUI = std::make_unique<TorrentTableUI>(torrentManager);
	torrentDetailsUI = std::make_unique<TorrentDetailsUI>();
	searchUI = std::make_unique<SearchUI>(searchEngine);
	modalDialogs = std::make_unique<ModalDialogs>(torrentManager);

	// Setup callbacks
	setupUICallbacks();
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

void UIManager::setupUICallbacks()
{
	// Setup torrent table callbacks
	torrentTableUI->setRemoveTorrentCallback([this](const lt::sha1_hash &hash, RemoveTorrentType removeType)
											 { torrentsToRemove.emplace_back(hash, removeType); });

	// Setup search UI callbacks
	searchUI->setSearchResultSelectedCallback([this](const TorrentSearchResult &result)
											  {
		modalDialogs->setSelectedSearchResult(result);
		// Open save path modal
		IGFD::FileDialogConfig config;
		config.path = defaultSavePath;
		config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowDevicesButton | ImGuiFileDialogFlags_DontShowHiddenFiles;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseSavePath", "Choose Save Path", nullptr, config); });

	searchUI->setShowFailurePopupCallback([this](const std::string &message)
										  { showFailurePopupWithMessage(message); });

	// Setup modal dialogs callbacks
	modalDialogs->setShowFailurePopupCallback([this](const std::string &message)
											  { showFailurePopupWithMessage(message); });

	modalDialogs->setRemoveCompletedCallback([this]()
											 { handleTorrentRemoval(); });
}

void UIManager::showFailurePopupWithMessage(const std::string &message)
{
	failurePopupMessage = message;
	showFailurePopup = true;
}

void UIManager::handleTorrentRemoval()
{
	for (const auto &removalInfo : torrentsToRemove)
	{
		Result result = torrentManager.removeTorrent(removalInfo.hash, removalInfo.removeType);
		if (!result)
		{
			showFailurePopupWithMessage(result.message);
		}
	}
	torrentsToRemove.clear();
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
	displayTorrentManagement();

	// Use the new TorrentDetailsUI component
	torrentDetailsUI->displayTorrentDetails(torrentTableUI->getSelectedTorrent());

	handleModals(showTorrentPopup, showMagnetTorrentPopup);

	if (showFailurePopup)
	{
		ImGui::OpenPopup("Failure");
		showFailurePopup = false;
	}
	modalDialogs->renderPopupFailure(failurePopupMessage);

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
	ImGui::DockBuilderDockWindow("Torrent Management", dock_top_id);
	ImGui::DockBuilderDockWindow("Torrent Details", dock_bottom_id);

	ImGui::DockBuilderFinish(dockspace_id);
}

void UIManager::handleModals(bool &showTorrentPopup, bool &showMagnetTorrentPopup)
{
	modalDialogs->handleAddTorrentModal(showTorrentPopup, defaultSavePath);
	modalDialogs->handleAddMagnetTorrentModal(showMagnetTorrentPopup);
	modalDialogs->handleRemoveTorrentModal(torrentsToRemove);

	auto torrentToAdd = modalDialogs->getTorrentToAdd();
	modalDialogs->handleAskSavePathModal(torrentToAdd, searchUI->getSelectedSearchResult(), defaultSavePath, "");
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
				 { exitRequested = true; }},
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

void UIManager::displayTorrentManagement()
{
	if (ImGui::Begin("Torrent Management"))
	{
		if (ImGui::BeginTabBar("TorrentManagementTabs", ImGuiTabBarFlags_None))
		{
			// My Torrents Tab
			if (ImGui::BeginTabItem("My Torrents"))
			{
				torrentTableUI->displayTorrentTable();
				ImGui::EndTabItem();
			}

			// Search Tab
			if (ImGui::BeginTabItem("Search Torrents"))
			{
				searchUI->displayIntegratedSearch();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	ImGui::End();
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
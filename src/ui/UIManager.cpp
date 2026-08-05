#include "UIManager.hpp"
#include "Theme.hpp"
#include "AppPaths.hpp"
#include "CredentialStore.hpp"
#include "Logger.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>

UIManager::UIManager(TorrentManager &torrentManager, SearchEngine &searchEngine, ConfigManager &settingsConfigManager)
	: torrentManager(torrentManager), searchEngine(searchEngine), settingsConfigManager(settingsConfigManager)
{
	const std::optional<std::string> torznabApiKey = Utils::CredentialStore::load("torznab_api_key");
	const char *environmentApiKey = std::getenv("HYPERTUBE_TORZNAB_API_KEY");
	const std::string initialApiKey = torznabApiKey.value_or(environmentApiKey ? environmentApiKey : "");
	std::snprintf(tempTorznabApiKey.data(), tempTorznabApiKey.size(), "%s", initialApiKey.c_str());
	const std::optional<std::string> proxyPassword = Utils::CredentialStore::load("proxy_password");
	std::snprintf(tempProxyPassword.data(), tempProxyPassword.size(), "%s", proxyPassword.value_or("").c_str());
	systemOpener = std::make_unique<Utils::SystemUtils::SystemOpener>();
	// Initialize UI components
	torrentTableUI = std::make_unique<TorrentTableUI>(torrentManager, *systemOpener);
	torrentDetailsUI = std::make_unique<TorrentDetailsUI>(torrentManager, *systemOpener);
	searchUI = std::make_unique<SearchUI>(searchEngine);
	modalDialogs = std::make_unique<ModalDialogs>(torrentManager);
	logsUI = std::make_unique<LogsUI>(torrentManager);

	// Setup callbacks
	setupUICallbacks();
}

UIManager::~UIManager()
{
	// A preference save can still be queued when the user closes the window.
	// Resolve it before the ConfigManager is destroyed so credentials cannot
	// remain changed when the durable candidate was rejected.
	if (!pendingPreferencesSave)
		return;
	const Result saveResult = pendingPreferencesSave->get();
	pendingPreferencesSave.reset();
	if (saveResult)
		settingsConfigManager.commitPreferences(pendingPreferences);
	else
		restorePreferenceCredentials();
}

void UIManager::init(GLFWwindow *window)
{
	initImGui(window);
	setDefaultSavePath();
	applySpeedLimits();
}

void UIManager::setDefaultSavePath()
{
	const std::string configuredPath = settingsConfigManager.getDownloadPath();
// Set defaultSavePath to downloads directory of the current user
#ifdef _WIN32
	const char *userProfile = std::getenv("USERPROFILE");
	if (!configuredPath.empty() && configuredPath != "~/Downloads")
	{
		this->defaultSavePath = configuredPath;
		return;
	}
	if (userProfile && std::strlen(userProfile) > 0)
	{
		this->defaultSavePath = std::string(userProfile) + "\\Downloads";
	}
	else
	{
		this->defaultSavePath = std::filesystem::current_path().string();
	}
#else
	const char *home = std::getenv("HOME");
	if (!configuredPath.empty() && configuredPath.rfind("~/", 0) != 0)
	{
		this->defaultSavePath = configuredPath;
		return;
	}
	if (home && std::strlen(home) > 0)
	{
		this->defaultSavePath = configuredPath.rfind("~/", 0) == 0
			? std::string(home) + configuredPath.substr(1)
			: std::string(home) + "/Downloads";
	}
	else
	{
		this->defaultSavePath = std::filesystem::current_path().string();
	}
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
	imguiIniPath = (Utils::AppPaths::configDirectory() / "imgui.ini").string();
	io_ref.IniFilename = imguiIniPath.c_str();
	HypertubeTheme::configureFonts(io_ref, 15.0f);
	this->io = io_ref;

	// Settings were loaded once by App before the UI is initialized.
	currentTheme = settingsConfigManager.getTheme();
	HypertubeTheme::applyTheme(static_cast<HypertubeTheme::ThemeType>(currentTheme));

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
	torrentTableUI->setRemoveTorrentCallback([this](const lt::info_hash_t &hash, const std::string &name, TorrentRemovalMode removeMode)
											 { torrentsToRemove.emplace_back(hash, name, removeMode); });
	torrentTableUI->setResultCallback([this](const Result &result)
										  {
		if (!result)
			showFailurePopupWithMessage(result.message);
	});
	torrentDetailsUI->setResultCallback([this](const Result &result)
	{
		if (!result)
			showFailurePopupWithMessage(result.message);
	});

	// Setup search UI callbacks
	searchUI->setSearchResultSelectedCallback([this](const TorrentSearchResult &result)
											  {
		modalDialogs->beginSearchResult(result); });

	searchUI->setShowFailurePopupCallback([this](const std::string &message)
										  { showFailurePopupWithMessage(message); });

	// Setup modal dialogs callbacks
	modalDialogs->setShowFailurePopupCallback([this](const std::string &message)
											  { showFailurePopupWithMessage(message); });

	modalDialogs->setRemoveCompletedCallback([this]()
											 { handleTorrentRemoval(); });

	modalDialogs->setRemoveCancelledCallback([this]()
											 { torrentsToRemove.clear(); });
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
		Result result = torrentManager.removeTorrent(removalInfo.hash, removalInfo.removeMode);
		if (!result)
		{
			showFailurePopupWithMessage(result.message);
		}
	}
	torrentsToRemove.clear();
}

void UIManager::renderFrame(GLFWwindow *window, const ImVec4 &clear_color)
{
	for (const auto &result : systemOpener->drainResults())
	{
		if (!result.result)
			showFailurePopupWithMessage(result.result.message);
	}
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

	// Reset popup triggers
	showMagnetTorrentPopup = false;
	showTorrentPopup = false;
	handleKeyboardShortcuts();

	displayMenuBar();

	displayCategories();
	searchUI->update();
	displayTorrentManagement();

	// Use the new TorrentDetailsUI component
	torrentDetailsUI->displayTorrentDetails(torrentTableUI->getSelectedTorrent());

	// Update and display logs
	logsUI->updateLogs();
	logsUI->displayLogsWindow();

	handleModals();

	displayPreferencesDialog();

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

void UIManager::handleKeyboardShortcuts()
{
	const ImGuiIO &frameIo = ImGui::GetIO();
	if (frameIo.WantTextInput)
		return;
	if (frameIo.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
		showTorrentPopup = true;
	if (frameIo.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_U, false))
		showMagnetTorrentPopup = true;
	if (frameIo.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
		showPreferencesDialog = true;
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

	// Split the bottom area to make room for logs
	ImGuiID dock_details_id;
	ImGuiID dock_logs_id;
	ImGui::DockBuilderSplitNode(dock_bottom_id, ImGuiDir_Right, 0.5f, &dock_logs_id, &dock_details_id);

	ImGui::DockBuilderDockWindow("Categories", dock_left_id);
	ImGui::DockBuilderDockWindow("Torrent Management", dock_top_id);
	ImGui::DockBuilderDockWindow("Torrent Details", dock_details_id);
	ImGui::DockBuilderDockWindow("Logs", dock_logs_id);

	ImGui::DockBuilderFinish(dockspace_id);
}

void UIManager::handleModals()
{
	modalDialogs->handleAddTorrentModal(showTorrentPopup, defaultSavePath);
	modalDialogs->handleAddMagnetTorrentModal(showMagnetTorrentPopup);
	modalDialogs->handleRemoveTorrentModal(torrentsToRemove);

	modalDialogs->handleAskSavePathModal(defaultSavePath, "");
}

void UIManager::displayMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			static const std::vector<MenuBarItem> menuItems = {
				{"Add a torrent...", "CTRL+O", [](UIManager *mgr)
				 { mgr->showTorrentPopup = true; }},
				{"Add a magnet link...", "CTRL+U", [](UIManager *mgr)
				 { mgr->showMagnetTorrentPopup = true; }},
				{"Preferences", "CTRL+P", [](UIManager *mgr)
				 { mgr->showPreferencesDialog = true; }},
				{"Exit", "ALT+F4", [](UIManager *mgr)
				 { mgr->exitRequested = true; }},
			};

			for (const auto &item : menuItems)
			{
				if (ImGui::MenuItem(item.label.c_str(), item.shortcut.c_str()))
				{
					item.action(this);
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

			// Favorites Tab
			if (ImGui::BeginTabItem("Favorites"))
			{
				searchUI->displayFavorites();
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
	torrentManager.requestStatusRefresh();

	// Section header
	HypertubeTheme::drawSectionHeader("Filter Torrents");
	for (const auto &category : torrentTableUI->getCategories())
	{
		if (HypertubeTheme::drawCategoryItem(category.label.c_str(), "", selectedCategory == category.id, category.count))
			selectedCategory = category.id;
	}

	torrentTableUI->setCategoryFilter(selectedCategory);

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

void UIManager::displayPreferencesDialog()
{
	if (showPreferencesDialog)
	{
		ImGui::OpenPopup("Preferences");
		showPreferencesDialog = false;
		// Load current values when opening dialog
		tempDownloadSpeedLimit = settingsConfigManager.getDownloadSpeedLimit();
		tempUploadSpeedLimit = settingsConfigManager.getUploadSpeedLimit();
		tempSelectedTheme = currentTheme;
		std::snprintf(tempDownloadPath.data(), tempDownloadPath.size(), "%s", defaultSavePath.c_str());
		tempEnableDht = settingsConfigManager.getEnableDHT();
		tempEnableUpnp = settingsConfigManager.getEnableUPnP();
		tempEnableNatPmp = settingsConfigManager.getEnableNATPMP();
		tempTorznabEnabled = settingsConfigManager.getTorznabEnabled();
		const std::string torznabUrl = settingsConfigManager.getTorznabUrl();
		std::snprintf(tempTorznabUrl.data(), tempTorznabUrl.size(), "%s", torznabUrl.c_str());
		tempProxyEnabled = settingsConfigManager.getProxyEnabled();
		tempProxyType = settingsConfigManager.getProxyType() == "http" ? 1 : 0;
		const std::string proxyHost = settingsConfigManager.getProxyHost();
		std::snprintf(tempProxyHost.data(), tempProxyHost.size(), "%s", proxyHost.c_str());
		tempProxyPort = settingsConfigManager.getProxyPort();
		const std::string proxyUsername = settingsConfigManager.getProxyUsername();
		std::snprintf(tempProxyUsername.data(), tempProxyUsername.size(), "%s", proxyUsername.c_str());
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
	ImGui::SetNextWindowSize(ImVec2(std::min(620.0f, workSize.x * 0.92f), std::min(760.0f, workSize.y * 0.92f)), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Preferences", nullptr))
	{
		if (pendingPreferencesSave)
		{
			finishPreferencesSave();
			if (pendingPreferencesSave)
			{
				ImGui::TextDisabled("Saving preferences...");
				ImGui::EndPopup();
				return;
			}
		}
		ImGui::BeginChild("PreferencesContent", ImVec2(0, -55), false);
		// Theme Section
		HypertubeTheme::drawSectionHeader("Appearance");

		ImGui::Text("Theme:");
		ImGui::SameLine(250);
		ImGui::SetNextItemWidth(170);
		const char *themeNames[] = {"Dark", "Ocean", "Nord", "Dracula", "CyberPunk"};
		if (ImGui::Combo("##ThemeCombo", &tempSelectedTheme, themeNames, IM_ARRAYSIZE(themeNames)))
		{
			// Preview theme immediately
			HypertubeTheme::applyTheme(static_cast<HypertubeTheme::ThemeType>(tempSelectedTheme));
		}
		HypertubeTheme::drawTooltip("Choose the color theme for the application");

		ImGui::Spacing();
		ImGui::Spacing();

		// Speed Limits Section
		HypertubeTheme::drawSectionHeader("Speed Limits");

		// Download speed limit
		ImGui::Text("Download Speed Limit (KB/s):");
		ImGui::SameLine(250);
		ImGui::SetNextItemWidth(120);
		int downloadKBps = tempDownloadSpeedLimit / 1024;
		if (ImGui::InputInt("##DownloadLimit", &downloadKBps, 1, 100))
		{
			if (downloadKBps < 0)
				downloadKBps = 0;
			tempDownloadSpeedLimit = downloadKBps * 1024;
		}
		HypertubeTheme::drawTooltip("Set to 0 for unlimited download speed");

		ImGui::Spacing();
		ImGui::Spacing();

		// Upload speed limit
		ImGui::Text("Upload Speed Limit (KB/s):");
		ImGui::SameLine(250);
		ImGui::SetNextItemWidth(120);
		int uploadKBps = tempUploadSpeedLimit / 1024;
		if (ImGui::InputInt("##UploadLimit", &uploadKBps, 1, 100))
		{
			if (uploadKBps < 0)
				uploadKBps = 0;
			tempUploadSpeedLimit = uploadKBps * 1024;
		}
		HypertubeTheme::drawTooltip("Set to 0 for unlimited upload speed");

		ImGui::Spacing();
		HypertubeTheme::drawSectionHeader("Downloads and discovery");
		ImGui::Text("Default download directory:");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##DownloadPath", tempDownloadPath.data(), tempDownloadPath.size());
		ImGui::Checkbox("DHT", &tempEnableDht);
		ImGui::SameLine();
		ImGui::Checkbox("UPnP", &tempEnableUpnp);
		ImGui::SameLine();
		ImGui::Checkbox("NAT-PMP", &tempEnableNatPmp);

		ImGui::Spacing();
		HypertubeTheme::drawSectionHeader("Search provider");
		ImGui::Checkbox("Enable local Torznab (Jackett/Prowlarr)", &tempTorznabEnabled);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##TorznabUrl", tempTorznabUrl.data(), tempTorznabUrl.size());
		ImGui::Text("API key:");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##TorznabApiKey", tempTorznabApiKey.data(), tempTorznabApiKey.size(), ImGuiInputTextFlags_Password);
		HypertubeTheme::drawTooltip("Stored in Windows Credential Manager, macOS Keychain, or Linux Secret Service; never in settings.json");

		ImGui::Spacing();
		HypertubeTheme::drawSectionHeader("Network proxy");
		ImGui::Checkbox("Route torrent and search traffic through a proxy", &tempProxyEnabled);
		const char *proxyTypes[] = {"SOCKS5", "HTTP"};
		ImGui::SetNextItemWidth(130);
		ImGui::Combo("Type", &tempProxyType, proxyTypes, IM_ARRAYSIZE(proxyTypes));
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("Host", tempProxyHost.data(), tempProxyHost.size());
		ImGui::InputInt("Port", &tempProxyPort);
		tempProxyPort = std::clamp(tempProxyPort, 1, 65535);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("Username", tempProxyUsername.data(), tempProxyUsername.size());
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("Password", tempProxyPassword.data(), tempProxyPassword.size(), ImGuiInputTextFlags_Password);
		HypertubeTheme::drawTooltip("The password is stored in the operating-system credential store");

		ImGui::EndChild();
		ImGui::Separator();
		ImGui::Spacing();

		// Centered buttons
		float buttonWidth = 130.0f;
		float spacing = 15.0f;
		float totalWidth = buttonWidth * 2 + spacing;
		float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
		ImGui::SetCursorPosX(startX);

		if (HypertubeTheme::drawStyledButton("Apply", ImVec2(buttonWidth, 35), true))
		{
			const std::string proxyType = tempProxyType == 1 ? "http" : "socks5";
			Result proxyValidation = SearchEngine::validateProxyConfig(tempProxyEnabled, proxyType,
				tempProxyHost.data(), tempProxyPort);
			if (!proxyValidation)
			{
				showFailurePopupWithMessage(proxyValidation.message);
				ImGui::EndPopup();
				return;
			}
			if (tempTorznabEnabled)
			{
				Result providerValidation = SearchEngine::validateTorznabConfig(tempTorznabUrl.data());
				if (!providerValidation)
				{
					showFailurePopupWithMessage(providerValidation.message);
					ImGui::EndPopup();
					return;
				}
			}
			previousPreferences = settingsConfigManager.getPreferencesSettings();
			previousTorznabCredential = Utils::CredentialStore::load("torznab_api_key");
			previousProxyCredential = Utils::CredentialStore::load("proxy_password");
			torznabCredentialChanged = false;
			proxyCredentialChanged = false;
			{
				Result credentialResult = !tempTorznabEnabled || tempTorznabApiKey[0] == '\0'
					? Utils::CredentialStore::erase("torznab_api_key")
					: Utils::CredentialStore::store("torznab_api_key", tempTorznabApiKey.data());
				if (!credentialResult)
				{
					showFailurePopupWithMessage(credentialResult.message);
					ImGui::EndPopup();
					return;
				}
				torznabCredentialChanged = true;
			}
			{
				Result credentialResult = !tempProxyEnabled || tempProxyPassword[0] == '\0'
					? Utils::CredentialStore::erase("proxy_password")
					: Utils::CredentialStore::store("proxy_password", tempProxyPassword.data());
				if (!credentialResult)
				{
					restorePreferenceCredentials();
					showFailurePopupWithMessage(credentialResult.message);
					ImGui::EndPopup();
					return;
				}
				proxyCredentialChanged = true;
			}
			pendingPreferences = {
				tempDownloadSpeedLimit,
				tempUploadSpeedLimit,
				tempSelectedTheme,
				tempDownloadPath.data(),
				tempEnableDht,
				tempEnableUpnp,
				tempEnableNatPmp,
				tempTorznabEnabled,
				tempTorznabUrl.data(),
				tempProxyEnabled,
				proxyType,
				tempProxyHost.data(),
				tempProxyPort,
				tempProxyUsername.data()};
			pendingPreferencesSave = settingsConfigManager.savePreferencesCandidate(
				Utils::AppPaths::settingsConfigPath().string(), pendingPreferences);
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine(0, spacing);
		if (HypertubeTheme::drawStyledButton("Cancel", ImVec2(buttonWidth, 35), false))
		{
			// Revert theme preview if cancelled
			HypertubeTheme::applyTheme(static_cast<HypertubeTheme::ThemeType>(currentTheme));
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

Result UIManager::restorePreferenceCredentials()
{
	Result firstFailure = Result::Success();
	if (torznabCredentialChanged)
	{
		Result result = previousTorznabCredential
			? Utils::CredentialStore::store("torznab_api_key", *previousTorznabCredential)
			: Utils::CredentialStore::erase("torznab_api_key");
		if (!result)
		{
			Utils::Logger::error("config", "Unable to restore Torznab credential: " + result.message);
			firstFailure = Result::Failure("Unable to restore Torznab credential: " + result.message, ResultCode::Partial);
		}
	}
	if (proxyCredentialChanged)
	{
		Result result = previousProxyCredential
			? Utils::CredentialStore::store("proxy_password", *previousProxyCredential)
			: Utils::CredentialStore::erase("proxy_password");
		if (!result)
		{
			Utils::Logger::error("config", "Unable to restore proxy credential: " + result.message);
			if (firstFailure)
				firstFailure = Result::Failure("Unable to restore proxy credential: " + result.message, ResultCode::Partial);
		}
	}
	torznabCredentialChanged = false;
	proxyCredentialChanged = false;
	return firstFailure;
}

Result UIManager::applyPreferencesRuntime(const PreferencesSettings &settings)
{
	currentTheme = settings.theme;
	HypertubeTheme::applyTheme(static_cast<HypertubeTheme::ThemeType>(currentTheme));
	defaultSavePath = settings.downloadPath;
	torrentManager.setDownloadSpeedLimit(settings.downloadSpeedLimit);
	torrentManager.setUploadSpeedLimit(settings.uploadSpeedLimit);
	torrentManager.configureDiscovery(settings.enableDht, settings.enableUpnp, settings.enableNatPmp);
	const std::string password = Utils::CredentialStore::load("proxy_password").value_or("");
	torrentManager.setProxyConfig(settings.proxyHost, settings.proxyPort, settings.proxyUsername, password,
		settings.proxyEnabled ? (settings.proxyType == "http" ? 2 : 1) : 0);
	Result proxyResult = searchEngine.setProxyConfig(settings.proxyEnabled, settings.proxyType,
		settings.proxyHost, settings.proxyPort, settings.proxyUsername, password);
	if (!proxyResult)
		return proxyResult;
	if (settings.torznabEnabled)
	{
		const std::string apiKey = Utils::CredentialStore::load("torznab_api_key").value_or("");
		Result provider = searchEngine.configureTorznabProvider(settings.torznabUrl, apiKey);
		if (!provider)
			return provider;
		searchEngine.setActiveSearchProvider("torznab");
	}
	else
		searchEngine.setActiveSearchProvider("torrents-csv");
	return Result::Success();
}

void UIManager::finishPreferencesSave()
{
	if (!pendingPreferencesSave || pendingPreferencesSave->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return;
	const Result saveResult = pendingPreferencesSave->get();
	pendingPreferencesSave.reset();
	if (!saveResult)
	{
		const Result restore = restorePreferenceCredentials();
		showFailurePopupWithMessage(restore ? "Preferences were not saved: " + saveResult.message
			: "Preferences were not saved and credentials could not be fully restored: " + restore.message);
		return;
	}

	Result commitResult = settingsConfigManager.commitPreferences(pendingPreferences);
	Result runtimeResult = commitResult ? applyPreferencesRuntime(pendingPreferences) : commitResult;
	if (!runtimeResult)
	{
		const Result credentialRestore = restorePreferenceCredentials();
		settingsConfigManager.commitPreferences(previousPreferences);
		const Result restoreRuntime = applyPreferencesRuntime(previousPreferences);
		settingsConfigManager.savePreferencesCandidate(Utils::AppPaths::settingsConfigPath().string(), previousPreferences);
		if (!restoreRuntime || !credentialRestore)
			showFailurePopupWithMessage("Preferences failed and runtime restoration was incomplete: " + runtimeResult.message);
		else
			showFailurePopupWithMessage("Preferences were rolled back: " + runtimeResult.message);
		return;
	}

	torznabCredentialChanged = false;
	proxyCredentialChanged = false;
	ImGui::CloseCurrentPopup();
}

void UIManager::applySpeedLimits()
{
	int downloadLimit = settingsConfigManager.getDownloadSpeedLimit();
	int uploadLimit = settingsConfigManager.getUploadSpeedLimit();

	torrentManager.setDownloadSpeedLimit(downloadLimit);
	torrentManager.setUploadSpeedLimit(uploadLimit);
}

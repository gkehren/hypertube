#include "ModalDialogs.hpp"
#include "UIManager.hpp"
#include <cstring>
#include <iostream>

ModalDialogs::ModalDialogs(TorrentManager &torrentManager)
	: torrentManager(torrentManager)
{
}

void ModalDialogs::handleAddTorrentModal(bool &showTorrentPopup, const std::string &defaultSavePath)
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
				torrentToAdd.first = false; // Not a magnet link
				torrentToAdd.second = filePath;
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void ModalDialogs::handleAddMagnetTorrentModal(bool &showMagnetTorrentPopup)
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
		ImGui::InputText("##MagnetLink", magnetLinkBuffer, IM_ARRAYSIZE(magnetLinkBuffer));

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			torrentToAdd.first = true; // Is a magnet link
			torrentToAdd.second = std::string(magnetLinkBuffer);
			memset(magnetLinkBuffer, 0, sizeof(magnetLinkBuffer));
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

void ModalDialogs::handleRemoveTorrentModal(const std::vector<TorrentRemovalInfo> &torrentsToRemove)
{
	if (torrentsToRemove.size() > 0)
	{
		ImGui::OpenPopup("Remove Torrent");
	}
	removeTorrentModal();
}

void ModalDialogs::handleAskSavePathModal(const std::pair<bool, std::string> &torrentToAdd,
										  const TorrentSearchResult &selectedSearchResult,
										  const std::string &defaultSavePath,
										  const std::string &currentSavePath)
{
	if (!torrentToAdd.second.empty())
	{
		IGFD::FileDialogConfig config;
		config.path = currentSavePath.empty() ? defaultSavePath : currentSavePath;
		config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowDevicesButton | ImGuiFileDialogFlags_DontShowHiddenFiles;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseSavePath", "Choose a directory to save the torrent", nullptr, config);
	}
	askSavePathModal();
}

void ModalDialogs::askSavePathModal()
{
	if (torrentToAdd.second.empty() && selectedSearchResult.infoHash.empty())
		return;

	if (ImGuiFileDialog::Instance()->Display("ChooseSavePath", ImGuiWindowFlags_NoCollapse, ImVec2(800, 600)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			savePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			if (!savePath.empty())
			{
				Result result(true);

				// Handle search result
				if (!selectedSearchResult.infoHash.empty())
				{
					result = torrentManager.addMagnetTorrent(selectedSearchResult.magnetUri, savePath);
					selectedSearchResult = TorrentSearchResult(); // Clear selection
				}
				// Handle regular torrent addition
				else if (!torrentToAdd.second.empty())
				{
					if (torrentToAdd.first)
					{
						result = torrentManager.addMagnetTorrent(torrentToAdd.second, savePath);
					}
					else
					{
						result = torrentManager.addTorrent(torrentToAdd.second, savePath);
					}
					torrentToAdd.second.clear();
				}

				if (!result)
				{
					if (onShowFailurePopup)
					{
						onShowFailurePopup(result.message);
					}
				}
			}
			ImGuiFileDialog::Instance()->Close();
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void ModalDialogs::renderPopupFailure(const std::string &message)
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

void ModalDialogs::removeTorrentModal()
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Remove Torrent", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Are you sure you want to remove the selected torrent?");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			if (onRemoveCompleted)
			{
				onRemoveCompleted();
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

void ModalDialogs::setShowFailurePopupCallback(std::function<void(const std::string &)> callback)
{
	onShowFailurePopup = callback;
}

void ModalDialogs::setTorrentAddCallback(std::function<void(const std::string &, const std::string &, bool)> callback)
{
	onTorrentAdd = callback;
}

void ModalDialogs::setRemoveCompletedCallback(std::function<void()> callback)
{
	onRemoveCompleted = callback;
}
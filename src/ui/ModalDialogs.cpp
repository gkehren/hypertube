#include "ModalDialogs.hpp"
#include "UIManager.hpp"
#include "Theme.hpp"
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
				addFlow.beginFile(filePath);
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
	ImGui::SetNextWindowSize(ImVec2(500, 180), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Add Magnet Torrent", nullptr, ImGuiWindowFlags_NoResize))
	{
		HypertubeTheme::drawSectionHeader("Magnet Link");

		ImGui::PushItemWidth(-1);
		ImGui::InputTextMultiline("##MagnetLink", magnetLinkBuffer, IM_ARRAYSIZE(magnetLinkBuffer),
								  ImVec2(-1, 50), ImGuiInputTextFlags_None);
		ImGui::PopItemWidth();

		ImGui::Spacing();
		ImGui::Spacing();

		float buttonWidth = 130.0f;
		float spacing = 15.0f;
		float totalWidth = buttonWidth * 2 + spacing;
		float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
		ImGui::SetCursorPosX(startX);

		if (HypertubeTheme::drawStyledButton("Add", ImVec2(buttonWidth, 35), true))
		{
			addFlow.beginMagnet(std::string(magnetLinkBuffer));
			memset(magnetLinkBuffer, 0, sizeof(magnetLinkBuffer));
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine(0, spacing);
		if (HypertubeTheme::drawStyledButton("Cancel", ImVec2(buttonWidth, 35), false))
		{
			memset(magnetLinkBuffer, 0, sizeof(magnetLinkBuffer));
			addFlow.cancel();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void ModalDialogs::handleRemoveTorrentModal(const std::vector<TorrentRemovalInfo> &torrentsToRemove)
{
	if (torrentsToRemove.size() > 0)
	{
		pendingRemovals = torrentsToRemove;
		ImGui::OpenPopup("Remove Torrent");
	}
	removeTorrentModal();
}

void ModalDialogs::handleAskSavePathModal(const std::string &defaultSavePath, const std::string &currentSavePath)
{
	if (addFlow.needsDestinationDialog())
	{
		IGFD::FileDialogConfig config;
		config.path = currentSavePath.empty() ? defaultSavePath : currentSavePath;
		config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowDevicesButton | ImGuiFileDialogFlags_DontShowHiddenFiles;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseSavePath", "Choose a directory to save the torrent", nullptr, config);
		addFlow.markDestinationDialogOpened();
	}
	askSavePathModal();
}

void ModalDialogs::askSavePathModal()
{
	if (!addFlow.hasPendingRequest())
		return;

	if (ImGuiFileDialog::Instance()->Display("ChooseSavePath", ImGuiWindowFlags_NoCollapse, ImVec2(800, 600)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			savePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			if (!savePath.empty())
			{
				const auto request = addFlow.take();
				Result result = request && request->source == TorrentAddSource::TorrentFile
					? torrentManager.addTorrent(request->value, savePath)
					: torrentManager.addMagnetTorrent(request ? request->value : std::string{}, savePath);

				if (!result)
				{
					if (onShowFailurePopup)
					{
						onShowFailurePopup(result.message);
					}
				}
			}
		}
		else
		{
			addFlow.cancel();
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void ModalDialogs::renderPopupFailure(const std::string &message)
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Failure", nullptr, ImGuiWindowFlags_NoResize))
	{
		const auto &palette = HypertubeTheme::getCurrentPalette();

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, palette.error);
		ImGui::TextWrapped("%s", message.c_str());
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float buttonWidth = 130.0f;
		float startX = (ImGui::GetWindowWidth() - buttonWidth) * 0.5f;
		ImGui::SetCursorPosX(startX);

		if (HypertubeTheme::drawStyledButton("OK", ImVec2(buttonWidth, 35), true))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

void ModalDialogs::removeTorrentModal()
{
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(480, 220), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Remove Torrent", nullptr, ImGuiWindowFlags_NoResize))
	{
		const auto &palette = HypertubeTheme::getCurrentPalette();

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, palette.warning);
		const auto &removal = pendingRemovals.front();
		const bool deleteData = removal.removeMode == TorrentRemovalMode::DeleteData || removal.removeMode == TorrentRemovalMode::DeleteDataAndSourceTorrent;
		const bool deleteSource = removal.removeMode == TorrentRemovalMode::DeleteSourceTorrent || removal.removeMode == TorrentRemovalMode::DeleteDataAndSourceTorrent;
		ImGui::TextWrapped("Remove '%s'?", removal.name.empty() ? "selected torrent" : removal.name.c_str());
		ImGui::PopStyleColor();
		ImGui::TextWrapped("Downloaded data: %s", deleteData ? "delete" : "keep");
		ImGui::TextWrapped("Source .torrent file: %s", deleteSource ? "delete" : "keep");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float buttonWidth = 130.0f;
		float spacing = 15.0f;
		float totalWidth = buttonWidth * 2 + spacing;
		float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
		ImGui::SetCursorPosX(startX);

		// Use error color for Remove button
		ImGui::PushStyleColor(ImGuiCol_Button, palette.error);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(palette.error.x * 1.1f, palette.error.y * 1.1f, palette.error.z * 1.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(palette.error.x * 0.9f, palette.error.y * 0.9f, palette.error.z * 0.9f, 1.0f));
		if (ImGui::Button("Remove", ImVec2(buttonWidth, 35)))
		{
			if (onRemoveCompleted)
			{
				onRemoveCompleted();
			}
			pendingRemovals.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleColor(3);

		ImGui::SetItemDefaultFocus();
		ImGui::SameLine(0, spacing);
		if (HypertubeTheme::drawStyledButton("Cancel", ImVec2(buttonWidth, 35), false))
		{
			if (onRemoveCancelled)
			{
				onRemoveCancelled();
			}
			pendingRemovals.clear();
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

void ModalDialogs::setRemoveCancelledCallback(std::function<void()> callback)
{
	onRemoveCancelled = callback;
}

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

enum class DialogSelectionStatus
{
	Selected,
	Cancelled,
	Unavailable,
	Failed
};

struct DialogSelection
{
	DialogSelectionStatus status = DialogSelectionStatus::Unavailable;
	std::optional<std::filesystem::path> path;
	std::string message;
};

class DialogService
{
public:
	virtual ~DialogService() = default;
	virtual DialogSelection openTorrentFile() = 0;
	virtual DialogSelection selectDirectory() = 0;
};

std::unique_ptr<DialogService> createDialogService();

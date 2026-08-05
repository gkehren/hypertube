#pragma once

#include <filesystem>
#include <memory>
#include <optional>

// Native dialogs remain behind this boundary so the Slint views do not own a
// platform file-picker implementation. The migration can provide a native
// adapter without changing the Slint components or TorrentAddFlow.
class DialogService
{
public:
	virtual ~DialogService() = default;
	virtual std::optional<std::filesystem::path> openTorrentFile() = 0;
	virtual std::optional<std::filesystem::path> selectDirectory() = 0;
};

std::unique_ptr<DialogService> createDialogService();

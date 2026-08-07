#include "DialogService.hpp"

#include <array>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

extern char **environ;
#endif

namespace
{
#ifndef _WIN32
DialogSelection runPicker(const std::string &program,
	const std::vector<std::string> &arguments)
{
	int pipeDescriptors[2] = {-1, -1};
	if (pipe(pipeDescriptors) != 0)
		return {DialogSelectionStatus::Failed, std::nullopt, "Unable to create a picker pipe"};

	std::vector<std::string> argumentStorage;
	argumentStorage.reserve(arguments.size() + 1);
	argumentStorage.push_back(program);
	argumentStorage.insert(argumentStorage.end(), arguments.begin(), arguments.end());
	std::vector<char *> argv;
	argv.reserve(argumentStorage.size() + 1);
	for (auto &argument : argumentStorage)
		argv.push_back(argument.data());
	argv.push_back(nullptr);

	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0)
	{
		close(pipeDescriptors[0]);
		close(pipeDescriptors[1]);
		return {DialogSelectionStatus::Failed, std::nullopt, "Unable to initialize picker process"};
	}
	posix_spawn_file_actions_addclose(&actions, pipeDescriptors[0]);
	posix_spawn_file_actions_adddup2(&actions, pipeDescriptors[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&actions, pipeDescriptors[1]);
	pid_t pid = -1;
	const int spawnError = posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	close(pipeDescriptors[1]);
	if (spawnError != 0)
	{
		close(pipeDescriptors[0]);
		return {spawnError == ENOENT ? DialogSelectionStatus::Unavailable : DialogSelectionStatus::Failed,
			std::nullopt, std::string("Unable to launch ") + program + ": " + std::strerror(spawnError)};
	}

	std::string output;
	std::array<char, 1024> buffer{};
	while (true)
	{
		const ssize_t count = read(pipeDescriptors[0], buffer.data(), buffer.size());
		if (count <= 0)
			break;
		if (output.size() + static_cast<std::size_t>(count) <= 32768)
			output.append(buffer.data(), static_cast<std::size_t>(count));
	}
	close(pipeDescriptors[0]);

	int status = 0;
	if (waitpid(pid, &status, 0) < 0)
		return {DialogSelectionStatus::Failed, std::nullopt, "Unable to wait for the picker"};
	if (!WIFEXITED(status))
		return {DialogSelectionStatus::Failed, std::nullopt, "The picker terminated unexpectedly"};
	if (WEXITSTATUS(status) == 1)
		return {DialogSelectionStatus::Cancelled, std::nullopt, "Picker cancelled"};
	if (WEXITSTATUS(status) != 0)
		return {DialogSelectionStatus::Failed, std::nullopt, "The picker failed to execute"};
	while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
		output.pop_back();
	if (output.empty())
		return {DialogSelectionStatus::Failed, std::nullopt, "The picker returned no path"};
	return {DialogSelectionStatus::Selected, std::filesystem::path(std::move(output)), {}};
}
#endif

#ifdef _WIN32
DialogSelection runWindowsPicker(bool directory)
{
	const HRESULT initialization = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	const bool shouldUninitialize = SUCCEEDED(initialization);
	if (FAILED(initialization) && initialization != RPC_E_CHANGED_MODE)
		return {DialogSelectionStatus::Failed, std::nullopt, "Unable to initialize the native picker"};

	IFileOpenDialog *dialog = nullptr;
	const HRESULT creation = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog));
	if (FAILED(creation))
	{
		if (shouldUninitialize)
			CoUninitialize();
		return {DialogSelectionStatus::Failed, std::nullopt, "Unable to create the native picker"};
	}

	FILEOPENDIALOGOPTIONS options = 0;
	dialog->GetOptions(&options);
	options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
	if (directory)
		options |= FOS_PICKFOLDERS;
	dialog->SetOptions(options);
	if (!directory)
	{
		const COMDLG_FILTERSPEC filter = {L"Torrent files", L"*.torrent"};
		dialog->SetFileTypes(1, &filter);
		dialog->SetFileTypeIndex(1);
		dialog->SetDefaultExtension(L"torrent");
	}

	std::optional<std::filesystem::path> selected;
	const HRESULT showResult = dialog->Show(nullptr);
	if (SUCCEEDED(showResult))
	{
		IShellItem *item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item)))
		{
			PWSTR displayName = nullptr;
			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &displayName)) && displayName)
			{
				selected = std::filesystem::path(displayName);
				CoTaskMemFree(displayName);
			}
			item->Release();
		}
	}
	dialog->Release();
	if (shouldUninitialize)
		CoUninitialize();
	if (selected)
		return {DialogSelectionStatus::Selected, std::move(selected), {}};
	if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED))
		return {DialogSelectionStatus::Cancelled, std::nullopt, "Picker cancelled"};
	return {DialogSelectionStatus::Failed, std::nullopt, "The native picker failed"};
}
#endif

class NativeDialogService final : public DialogService
{
public:
	DialogSelection openTorrentFile() override
	{
#ifdef _WIN32
		return runWindowsPicker(false);
#elif defined(__APPLE__)
		return runPicker("osascript", {"-e", "POSIX path of (choose file of type {\"org.bittorrent.torrent\"})"});
#elif defined(__linux__)
		const auto zenity = runPicker("zenity", {"--file-selection", "--title=Select torrent file",
			"--file-filter=Torrent files | *.torrent"});
		if (zenity.status != DialogSelectionStatus::Unavailable)
			return zenity;
		return runPicker("kdialog", {"--getopenfilename", ".", "*.torrent", "Select torrent file"});
#else
		return {DialogSelectionStatus::Unavailable, std::nullopt, "No native file picker is available"};
#endif
	}

	DialogSelection selectDirectory() override
	{
#ifdef _WIN32
		return runWindowsPicker(true);
#elif defined(__APPLE__)
		return runPicker("osascript", {"-e", "POSIX path of (choose folder)"});
#elif defined(__linux__)
		const auto zenity = runPicker("zenity", {"--file-selection", "--directory", "--title=Select save directory"});
		if (zenity.status != DialogSelectionStatus::Unavailable)
			return zenity;
		return runPicker("kdialog", {"--getexistingdirectory", ".", "Select save directory"});
#else
		return {DialogSelectionStatus::Unavailable, std::nullopt, "No native directory picker is available"};
#endif
	}
};
}

std::unique_ptr<DialogService> createDialogService()
{
	return std::make_unique<NativeDialogService>();
}

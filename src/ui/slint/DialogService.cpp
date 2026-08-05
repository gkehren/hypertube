#include "DialogService.hpp"

#include <array>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
#ifndef _WIN32
std::optional<std::filesystem::path> runPicker(const std::string &program,
	const std::vector<std::string> &arguments)
{
	int pipeDescriptors[2] = {-1, -1};
	if (pipe(pipeDescriptors) != 0)
		return std::nullopt;

	const pid_t pid = fork();
	if (pid == 0)
	{
		close(pipeDescriptors[0]);
		if (dup2(pipeDescriptors[1], STDOUT_FILENO) < 0)
			_exit(127);
		close(pipeDescriptors[1]);
		const int nullDescriptor = open("/dev/null", O_WRONLY);
		if (nullDescriptor >= 0)
		{
			dup2(nullDescriptor, STDERR_FILENO);
			close(nullDescriptor);
		}

		std::vector<char *> argv;
		argv.reserve(arguments.size() + 2);
		argv.push_back(const_cast<char *>(program.c_str()));
		for (const auto &argument : arguments)
			argv.push_back(const_cast<char *>(argument.c_str()));
		argv.push_back(nullptr);
		execvp(program.c_str(), argv.data());
		_exit(127);
	}
	if (pid < 0)
	{
		close(pipeDescriptors[0]);
		close(pipeDescriptors[1]);
		return std::nullopt;
	}

	close(pipeDescriptors[1]);
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
	if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return std::nullopt;
	while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
		output.pop_back();
	if (output.empty())
		return std::nullopt;
	return std::filesystem::path(std::move(output));
}
#endif

#ifdef _WIN32
std::optional<std::filesystem::path> runWindowsPicker(bool directory)
{
	const HRESULT initialization = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	const bool shouldUninitialize = SUCCEEDED(initialization);
	if (FAILED(initialization) && initialization != RPC_E_CHANGED_MODE)
		return std::nullopt;

	IFileOpenDialog *dialog = nullptr;
	const HRESULT creation = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&dialog));
	if (FAILED(creation))
	{
		if (shouldUninitialize)
			CoUninitialize();
		return std::nullopt;
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
	if (SUCCEEDED(dialog->Show(nullptr)))
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
	return selected;
}
#endif

class NativeDialogService final : public DialogService
{
public:
	std::optional<std::filesystem::path> openTorrentFile() override
	{
#ifdef _WIN32
		return runWindowsPicker(false);
#elif defined(__APPLE__)
		return runPicker("osascript", {"-e", "POSIX path of (choose file of type {\"org.bittorrent.torrent\"})"});
#elif defined(__linux__)
		if (const auto path = runPicker("zenity", {"--file-selection", "--title=Select torrent file",
			"--file-filter=Torrent files | *.torrent"}))
			return path;
		return runPicker("kdialog", {"--getopenfilename", ".", "*.torrent", "Select torrent file"});
#else
		return std::nullopt;
#endif
	}

	std::optional<std::filesystem::path> selectDirectory() override
	{
#ifdef _WIN32
		return runWindowsPicker(true);
#elif defined(__APPLE__)
		return runPicker("osascript", {"-e", "POSIX path of (choose folder)"});
#elif defined(__linux__)
		if (const auto path = runPicker("zenity", {"--file-selection", "--directory", "--title=Select save directory"}))
			return path;
		return runPicker("kdialog", {"--getexistingdirectory", ".", "Select save directory"});
#else
		return std::nullopt;
#endif
	}
};
}

std::unique_ptr<DialogService> createDialogService()
{
	return std::make_unique<NativeDialogService>();
}

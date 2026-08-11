#include "AppPaths.hpp"

#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace
{
std::optional<bool> g_isPortableCached;
std::filesystem::path g_portableRootCached;
std::filesystem::path g_overrideExeDir;
std::mutex g_pathsMutex;

std::filesystem::path querySystemExecutablePath()
{
#if defined(_WIN32)
	wchar_t buffer[MAX_PATH];
	DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
	if (len > 0)
	{
		return std::filesystem::path(buffer);
	}
#elif defined(__APPLE__)
	char buffer[1024];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0)
	{
		std::error_code ec;
		auto canonical = std::filesystem::canonical(buffer, ec);
		return ec ? std::filesystem::path(buffer) : canonical;
	}
#else
	char buffer[PATH_MAX];
	ssize_t len = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (len > 0)
	{
		buffer[len] = '\0';
		return std::filesystem::path(buffer);
	}
#endif
	return std::filesystem::current_path();
}

std::filesystem::path envPath(const char *name)
{
	const char *value = std::getenv(name);
	return value && value[0] != '\0' ? std::filesystem::path(value) : std::filesystem::path();
}

std::filesystem::path homeDirectory()
{
#ifdef _WIN32
	if (auto path = envPath("USERPROFILE"); !path.empty())
		return path;
	return envPath("HOMEDRIVE") / envPath("HOMEPATH");
#else
	return envPath("HOME");
#endif
}

void resolvePortableStateUnlocked()
{
	if (g_isPortableCached.has_value())
		return;

	const auto exeDir = g_overrideExeDir.empty() ? querySystemExecutablePath().parent_path() : g_overrideExeDir;

	if (const char *portable = std::getenv("HYPERTUBE_PORTABLE"); portable && std::string(portable) == "1")
	{
		g_isPortableCached = true;
		g_portableRootCached = exeDir;
		return;
	}

	std::error_code ec;
	if (std::filesystem::exists(exeDir / "portable.mode", ec))
	{
		g_isPortableCached = true;
		g_portableRootCached = exeDir;
		return;
	}

	g_isPortableCached = false;
	g_portableRootCached.clear();
}
} // namespace

namespace Utils
{
void AppPaths::resetPortableCache()
{
	std::lock_guard<std::mutex> lock(g_pathsMutex);
	g_isPortableCached.reset();
	g_portableRootCached.clear();
	g_overrideExeDir.clear();
}

void AppPaths::setOverrideExecutableDirectory(const std::filesystem::path &path)
{
	std::lock_guard<std::mutex> lock(g_pathsMutex);
	g_overrideExeDir = path;
	g_isPortableCached.reset();
	g_portableRootCached.clear();
}

std::filesystem::path AppPaths::executableDirectory()
{
	std::lock_guard<std::mutex> lock(g_pathsMutex);
	if (!g_overrideExeDir.empty())
		return g_overrideExeDir;
	return querySystemExecutablePath().parent_path();
}

bool AppPaths::isPortable()
{
	std::lock_guard<std::mutex> lock(g_pathsMutex);
	resolvePortableStateUnlocked();
	return g_isPortableCached.value_or(false);
}

std::filesystem::path AppPaths::configDirectory()
{
	if (isPortable())
	{
		std::lock_guard<std::mutex> lock(g_pathsMutex);
		return g_portableRootCached / "config";
	}

#ifdef _WIN32
	if (auto path = envPath("APPDATA"); !path.empty())
		return path / "Hypertube";
#elif defined(__APPLE__)
	if (auto home = homeDirectory(); !home.empty())
		return home / "Library" / "Application Support" / "Hypertube";
#else
	if (auto path = envPath("XDG_CONFIG_HOME"); !path.empty())
		return path / "hypertube";
	if (auto home = homeDirectory(); !home.empty())
		return home / ".config" / "hypertube";
#endif
	return executableDirectory() / "config";
}

std::filesystem::path AppPaths::dataDirectory()
{
	if (isPortable())
	{
		std::lock_guard<std::mutex> lock(g_pathsMutex);
		return g_portableRootCached / "data";
	}

#ifdef _WIN32
	if (auto path = envPath("LOCALAPPDATA"); !path.empty())
		return path / "Hypertube";
#elif defined(__APPLE__)
	if (auto home = homeDirectory(); !home.empty())
		return home / "Library" / "Application Support" / "Hypertube";
#else
	if (auto path = envPath("XDG_DATA_HOME"); !path.empty())
		return path / "hypertube";
	if (auto home = homeDirectory(); !home.empty())
		return home / ".local" / "share" / "hypertube";
#endif
	return executableDirectory() / "data";
}

std::filesystem::path AppPaths::cacheDirectory()
{
	if (isPortable())
	{
		std::lock_guard<std::mutex> lock(g_pathsMutex);
		return g_portableRootCached / "cache";
	}

#ifdef _WIN32
	if (auto path = envPath("LOCALAPPDATA"); !path.empty())
		return path / "Hypertube" / "cache";
#elif defined(__APPLE__)
	if (auto home = homeDirectory(); !home.empty())
		return home / "Library" / "Caches" / "Hypertube";
#else
	if (auto path = envPath("XDG_CACHE_HOME"); !path.empty())
		return path / "hypertube";
	if (auto home = homeDirectory(); !home.empty())
		return home / ".cache" / "hypertube";
#endif
	return executableDirectory() / "cache";
}

std::filesystem::path AppPaths::expandUserPath(const std::filesystem::path &path)
{
	if (path.empty() || path.is_absolute())
		return path;

	const std::string genericPath = path.generic_string();
	if (genericPath != "~" && genericPath.rfind("~/", 0) != 0)
		return path.lexically_normal();

	const std::filesystem::path suffix = genericPath == "~"
		? std::filesystem::path()
		: std::filesystem::path(genericPath.substr(2));
	const std::filesystem::path home = homeDirectory();
	const std::filesystem::path base = home.empty() ? executableDirectory() : home;
	const std::filesystem::path normalizedBase = base.lexically_normal();
	return suffix.empty() ? normalizedBase : (normalizedBase / suffix).lexically_normal();
}

std::filesystem::path AppPaths::torrentsConfigPath()
{
	return configDirectory() / "torrents.json";
}

std::filesystem::path AppPaths::settingsConfigPath()
{
	return configDirectory() / "settings.json";
}

std::filesystem::path AppPaths::logFilePath()
{
	return dataDirectory() / "hypertube.log";
}

Result AppPaths::ensureDirectories()
{
	std::error_code error;
	const auto configDir = configDirectory();
	std::filesystem::create_directories(configDir, error);
	if (error)
		return Result::Failure("Failed to create configuration directory '" + configDir.string() + "': " + error.message(), ResultCode::Storage);

	const auto dataDir = dataDirectory();
	std::filesystem::create_directories(dataDir, error);
	if (error)
		return Result::Failure("Failed to create data directory '" + dataDir.string() + "': " + error.message(), ResultCode::Storage);

	const auto cacheDir = cacheDirectory();
	std::filesystem::create_directories(cacheDir, error);
	if (error)
		return Result::Failure("Failed to create cache directory '" + cacheDir.string() + "': " + error.message(), ResultCode::Storage);

	return Result::Success();
}
} // namespace Utils

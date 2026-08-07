#include "AppPaths.hpp"

#include <cstdlib>
#include <string>

namespace
{
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
} // namespace

namespace Utils
{
bool AppPaths::isPortable()
{
	if (const char *portable = std::getenv("HYPERTUBE_PORTABLE"); portable && std::string(portable) == "1")
		return true;
	return std::filesystem::exists(std::filesystem::current_path() / "portable.mode");
}

std::filesystem::path AppPaths::configDirectory()
{
	if (isPortable())
		return std::filesystem::current_path() / "config";

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
	return std::filesystem::current_path() / "config";
}

std::filesystem::path AppPaths::dataDirectory()
{
	if (isPortable())
		return std::filesystem::current_path() / "data";

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
	return std::filesystem::current_path() / "data";
}

std::filesystem::path AppPaths::cacheDirectory()
{
	if (isPortable())
		return std::filesystem::current_path() / "cache";

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
	return std::filesystem::current_path() / "cache";
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
	const std::filesystem::path base = home.empty() ? std::filesystem::current_path() : home;
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

void AppPaths::ensureDirectories()
{
	std::error_code error;
	std::filesystem::create_directories(configDirectory(), error);
	std::filesystem::create_directories(dataDirectory(), error);
	std::filesystem::create_directories(cacheDirectory(), error);
}
} // namespace Utils

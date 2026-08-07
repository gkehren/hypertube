#pragma once

#include "Result.hpp"
#include <filesystem>

namespace Utils
{
/**
 * Resolves the directories used by Hypertube.
 *
 * A portable installation can opt in by placing a `portable.mode` marker next
 * to the executable or setting HYPERTUBE_PORTABLE=1. In that mode all state stays
 * next to the executable directory. Otherwise per-user data directories are used.
 */
class AppPaths
{
public:
	static bool isPortable();
	static std::filesystem::path executableDirectory();
	static std::filesystem::path configDirectory();
	static std::filesystem::path dataDirectory();
	static std::filesystem::path cacheDirectory();
	static std::filesystem::path expandUserPath(const std::filesystem::path &path);
	static std::filesystem::path torrentsConfigPath();
	static std::filesystem::path settingsConfigPath();
	static std::filesystem::path logFilePath();
	static Result ensureDirectories();
	static void resetPortableCache();
	static void setOverrideExecutableDirectory(const std::filesystem::path &path);
};
} // namespace Utils

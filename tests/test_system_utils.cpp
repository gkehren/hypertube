#include <gtest/gtest.h>
#include "AppPaths.hpp"
#include "SystemUtils.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {

std::filesystem::path configuredHome()
{
#ifdef _WIN32
	if (const char *profile = std::getenv("USERPROFILE"); profile && profile[0] != '\0')
		return profile;
	const char *drive = std::getenv("HOMEDRIVE");
	const char *path = std::getenv("HOMEPATH");
	return drive && path ? std::filesystem::path(drive) / path : std::filesystem::path();
#else
	if (const char *home = std::getenv("HOME"); home && home[0] != '\0')
		return home;
	return {};
#endif
}

TEST(AppPathsTest, ExpandsTildeAgainstThePlatformHome)
{
	const auto home = configuredHome();
	if (home.empty())
		GTEST_SKIP() << "No platform home directory is configured";

	EXPECT_EQ(Utils::AppPaths::expandUserPath("~"), home.lexically_normal());
	EXPECT_EQ(Utils::AppPaths::expandUserPath("~/Downloads"), (home / "Downloads").lexically_normal());
}

TEST(AppPathsTest, NormalizesRelativePathsAndPreservesAbsolutePaths)
{
	EXPECT_EQ(Utils::AppPaths::expandUserPath("./downloads"), std::filesystem::path("downloads"));

	const auto absolute = std::filesystem::temp_directory_path() / "hypertube-absolute-downloads";
	EXPECT_EQ(Utils::AppPaths::expandUserPath(absolute), absolute);

	const std::filesystem::path unicode = "hypertube-unicode-\xC3\xA9";
	EXPECT_EQ(Utils::AppPaths::expandUserPath(unicode), unicode.lexically_normal());
}

#ifdef _WIN32
TEST(AppPathsTest, PreservesWindowsAbsolutePaths)
{
	const std::filesystem::path absolute = R"(C:\Users\Example\Downloads)";
	EXPECT_EQ(Utils::AppPaths::expandUserPath(absolute), absolute);
}
#endif

TEST(SystemOpenerTest, RejectsMissingPathsBeforeQueueing)
{
	Utils::SystemUtils::SystemOpener opener;
	EXPECT_EQ(opener.enqueueExplorer("/path/that/does/not/exist").code, ResultCode::NotFound);
	EXPECT_EQ(opener.enqueuePreview("/path/that/does/not/exist").code, ResultCode::NotFound);
	EXPECT_TRUE(opener.drainResults().empty());
}

TEST(SystemOpenerTest, RejectsEmptyPaths)
{
	Utils::SystemUtils::SystemOpener opener;
	EXPECT_FALSE(opener.enqueueExplorer("").success);
	EXPECT_FALSE(opener.enqueuePreview("").success);
}

TEST(SystemOpenerTest, DrainsInjectedSuccessAndFailureResults)
{
	const auto directory = std::filesystem::temp_directory_path() / "hypertube-system-opener-test";
	const auto file = directory / "sample.txt";
	const auto failingFile = directory / "fail.txt";
	std::filesystem::create_directories(directory);
	std::ofstream(file) << "sample";
	std::ofstream(failingFile) << "sample";
	Utils::SystemUtils::SystemOpener opener(4,
		[](Utils::SystemUtils::OpenOperationKind, const std::string &path) {
			return path.find("fail") != std::string::npos
				? Result::Failure("Injected opener failure", ResultCode::Unavailable)
				: Result::Success();
		});
	ASSERT_TRUE(opener.enqueueExplorer(directory.string()));
	ASSERT_TRUE(opener.enqueuePreview(file.string()));
	ASSERT_TRUE(opener.enqueuePreview(failingFile.string()));

	std::vector<Utils::SystemUtils::OpenOperationResult> results;
	for (int attempt = 0; attempt < 100 && results.size() < 3; ++attempt)
	{
		auto drained = opener.drainResults();
		results.insert(results.end(), drained.begin(), drained.end());
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	ASSERT_EQ(results.size(), 3U);
	EXPECT_TRUE(results[0].result);
	EXPECT_TRUE(results[1].result);
	EXPECT_FALSE(results[2].result);
	std::error_code error;
	std::filesystem::remove_all(directory, error);
}

TEST(SystemUtilsTest, LegacyHelpersValidatePaths)
{
	EXPECT_EQ(Utils::SystemUtils::openFileExplorer("/path/that/does/not/exist").code, ResultCode::NotFound);
	EXPECT_EQ(Utils::SystemUtils::openFilePreview("/path/that/does/not/exist").code, ResultCode::NotFound);
}

TEST(SystemOpenerTest, ConstructionDestructionStress)
{
	for (int i = 0; i < 100; ++i)
	{
		Utils::SystemUtils::SystemOpener opener(16, [](Utils::SystemUtils::OpenOperationKind, const std::string &) {
			return Result::Success();
		});
	}
}

TEST(SystemOpenerTest, DestroysCleanlyWithPendingWork)
{
	const auto tempFile = std::filesystem::temp_directory_path() / "hypertube-pending-test.txt";
	std::ofstream(tempFile) << "content";

	{
		Utils::SystemUtils::SystemOpener opener(16, [](Utils::SystemUtils::OpenOperationKind, const std::string &) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			return Result::Success();
		});

		for (int i = 0; i < 5; ++i)
		{
			EXPECT_TRUE(opener.enqueuePreview(tempFile.string()));
		}
	}

	std::error_code error;
	std::filesystem::remove(tempFile, error);
}

} // namespace

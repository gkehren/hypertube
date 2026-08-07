#include <gtest/gtest.h>
#include "CredentialStore.hpp"
#include "AppPaths.hpp"
#include "SystemUtils.hpp"
#include "Logger.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {

TEST(CredentialStoreTest, AsyncRefreshHasAnExplicitShutdownPath)
{
	Utils::CredentialStore::asyncRefreshStatus({});
	Utils::CredentialStore::asyncRefreshStatus({});
	Utils::CredentialStore::shutdown();
	SUCCEED();
}

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

TEST(AppPathsTest, StablePortableRootAndDirectoryCreation)
{
	Utils::AppPaths::resetPortableCache();
	const auto cwd = std::filesystem::current_path();
	Utils::AppPaths::setOverrideExecutableDirectory(cwd);
	const auto marker = cwd / "portable.mode";
	std::ofstream(marker) << "";

	EXPECT_TRUE(Utils::AppPaths::isPortable());
	const auto configDir = Utils::AppPaths::configDirectory();
	EXPECT_EQ(configDir, cwd / "config");

	const auto tempSubdir = cwd / "temp_subdir_test";
	std::filesystem::create_directories(tempSubdir);
	std::filesystem::current_path(tempSubdir);

	EXPECT_TRUE(Utils::AppPaths::isPortable());
	EXPECT_EQ(Utils::AppPaths::configDirectory(), cwd / "config");

	std::filesystem::current_path(cwd);
	std::error_code ec;
	std::filesystem::remove(marker, ec);
	std::filesystem::remove_all(tempSubdir, ec);
	Utils::AppPaths::resetPortableCache();
}

TEST(AppPathsTest, PortableModeUsesExecutableDirectoryWhenCwdDiffers)
{
	Utils::AppPaths::resetPortableCache();
	const auto tempExeDir = std::filesystem::temp_directory_path() / "hypertube_test_exedir";
	const auto tempCwd = std::filesystem::temp_directory_path() / "hypertube_test_cwd";
	std::filesystem::create_directories(tempExeDir);
	std::filesystem::create_directories(tempCwd);

	const auto marker = tempExeDir / "portable.mode";
	std::ofstream(marker) << "";

	Utils::AppPaths::setOverrideExecutableDirectory(tempExeDir);
	const auto originalCwd = std::filesystem::current_path();
	std::filesystem::current_path(tempCwd);

	EXPECT_TRUE(Utils::AppPaths::isPortable());
	EXPECT_EQ(Utils::AppPaths::configDirectory(), tempExeDir / "config");
	EXPECT_EQ(Utils::AppPaths::dataDirectory(), tempExeDir / "data");

	std::filesystem::current_path(originalCwd);
	std::error_code ec;
	std::filesystem::remove_all(tempExeDir, ec);
	std::filesystem::remove_all(tempCwd, ec);
	Utils::AppPaths::resetPortableCache();
}

TEST(AppPathsTest, IgnoresPortableModeMarkerInCurrentWorkingDirectoryIfExecutableDirDiffers)
{
	Utils::AppPaths::resetPortableCache();
	const auto tempExeDir = std::filesystem::temp_directory_path() / "hypertube_app_dir";
	const auto tempCwd = std::filesystem::temp_directory_path() / "hypertube_unrelated_cwd";
	std::filesystem::create_directories(tempExeDir);
	std::filesystem::create_directories(tempCwd);

	const auto cwdMarker = tempCwd / "portable.mode";
	std::ofstream(cwdMarker) << "";

	Utils::AppPaths::setOverrideExecutableDirectory(tempExeDir);
	const auto originalCwd = std::filesystem::current_path();
	std::filesystem::current_path(tempCwd);

	EXPECT_FALSE(Utils::AppPaths::isPortable());

	std::filesystem::current_path(originalCwd);
	std::error_code ec;
	std::filesystem::remove_all(tempExeDir, ec);
	std::filesystem::remove_all(tempCwd, ec);
	Utils::AppPaths::resetPortableCache();
}

TEST(LoggerRedactionTest, RedactsSensitiveTokensFromDiagnosticsExport)
{
	const auto tempLog = std::filesystem::temp_directory_path() / "hypertube_test_redaction.log";
	Utils::Logger::initialize(tempLog);

	const std::string secret1 = "SUPER_SECRET_API_TOKEN_123";
	const std::string secret2 = "MY_PROXY_PASSWORD_456";
	const std::string secret3 = "URL_SECRET_789";
	const std::string secret4 = "BEARER_SECRET_abc";
	const std::string secret5 = "BASIC_SECRET_def";
	const std::string secret6 = "API_HEADER_SECRET_ghi";
	const std::string secret7 = "ACCESS_TOKEN_SECRET_jkl";

	Utils::Logger::info("test", "Connected with api_key=" + secret1);
	Utils::Logger::info("test", "PROXY_PASSWORD=" + secret2);
	Utils::Logger::info("test", "Endpoint https://user:" + secret3 + "@proxy.example.com");
	Utils::Logger::info("test", "Authorization: Bearer " + secret4);
	Utils::Logger::info("test", "Authorization: Basic " + secret5);
	Utils::Logger::info("test", "X-API-Key: " + secret6);
	Utils::Logger::info("test", "Request /lookup?access_token: " + secret7 + "&q=test");

	const std::string diag = Utils::Logger::formatDiagnostics();

	EXPECT_EQ(diag.find(secret1), std::string::npos);
	EXPECT_EQ(diag.find(secret2), std::string::npos);
	EXPECT_EQ(diag.find(secret3), std::string::npos);
	EXPECT_EQ(diag.find(secret4), std::string::npos);
	EXPECT_EQ(diag.find(secret5), std::string::npos);
	EXPECT_EQ(diag.find(secret6), std::string::npos);
	EXPECT_EQ(diag.find(secret7), std::string::npos);

	std::error_code ec;
	std::filesystem::remove(tempLog, ec);
}

TEST(AppPathsTest, EnsureDirectoriesReturnsSuccess)
{
	const auto res = Utils::AppPaths::ensureDirectories();
	EXPECT_TRUE(res);
}

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

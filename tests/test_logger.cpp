#include <gtest/gtest.h>
#include "Logger.hpp"
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		fs::path tempBase = fs::temp_directory_path();
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(1000, 9999);
		testDir = tempBase / ("hypertube_logger_test_" + std::to_string(dis(gen)));
		fs::create_directories(testDir);
	}

	void TearDown() override
	{
		if (fs::exists(testDir))
		{
			fs::remove_all(testDir);
		}
	}

	fs::path testDir;
};

TEST_F(LoggerTest, BoundedLogRotation)
{
	const auto logPath = testDir / "test.log";
	Utils::Logger::initialize(logPath);
	Utils::Logger::setMaxLogSize(100);
	Utils::Logger::setMaxBackupFiles(2);

	for (int i = 0; i < 20; ++i)
	{
		Utils::Logger::info("test", "Log entry iteration " + std::to_string(i) + " with extra padding text");
	}

	EXPECT_TRUE(fs::exists(logPath));
	EXPECT_TRUE(fs::exists(logPath.string() + ".1"));
	EXPECT_TRUE(fs::exists(logPath.string() + ".2"));
	EXPECT_FALSE(fs::exists(logPath.string() + ".3"));
}

TEST_F(LoggerTest, FormatAndExportDiagnostics)
{
	const auto logPath = testDir / "diag.log";
	Utils::Logger::initialize(logPath);
	Utils::Logger::clearRecent();

	Utils::Logger::info("app", "Application started");
	Utils::Logger::error("network", "Connection timeout");

	std::string formatted = Utils::Logger::formatDiagnostics();
	EXPECT_NE(formatted.find("Application started"), std::string::npos);
	EXPECT_NE(formatted.find("Connection timeout"), std::string::npos);
	EXPECT_NE(formatted.find("Hypertube Application Diagnostics Report"), std::string::npos);

	const auto exportPath = testDir / "exported_diagnostics.txt";
	std::string error;
	ASSERT_TRUE(Utils::Logger::exportDiagnosticsToFile(exportPath, error));
	EXPECT_TRUE(fs::exists(exportPath));

	std::ifstream file(exportPath);
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	EXPECT_EQ(content, formatted);
}

TEST_F(LoggerTest, RedactsSensitiveDataInDiagnosticsReport)
{
	const auto logPath = testDir / "redact.log";
	Utils::Logger::initialize(logPath);
	Utils::Logger::clearRecent();

	Utils::Logger::info("network", "Request url https://indexer.local/api?api_key=secretkey12345&q=linux");
	Utils::Logger::error("proxy", "Connect failed for user admin pass=supersecretpass123");

	std::string report = Utils::Logger::formatDiagnostics();
	EXPECT_EQ(report.find("secretkey12345"), std::string::npos);
	EXPECT_EQ(report.find("supersecretpass123"), std::string::npos);
	EXPECT_NE(report.find("api_key=[REDACTED]"), std::string::npos);
	EXPECT_NE(report.find("pass=[REDACTED]"), std::string::npos);
}

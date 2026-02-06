#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <random>
#include "../include/app/ConfigManager.hpp"

namespace fs = std::filesystem;

class ConfigManagerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Create a unique temporary directory for test configs
		fs::path tempBase = fs::temp_directory_path();
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(1000, 9999);
		testDir = tempBase / ("hypertube_config_test_" + std::to_string(dis(gen)));
		fs::create_directories(testDir);
	}

	void TearDown() override
	{
		// Clean up test directory
		if (fs::exists(testDir))
		{
			fs::remove_all(testDir);
		}
	}

	fs::path testDir;
};

TEST_F(ConfigManagerTest, CreateDefaultConfig)
{
	ConfigManager manager;
	std::string configPath = (testDir / "settings.json").string();

	// Load config (should create default since file doesn't exist)
	manager.load(configPath);

	// Check version
	EXPECT_EQ(manager.getConfigVersion(), 1);

	// Check default values
	EXPECT_EQ(manager.getDownloadSpeedLimit(), 0);
	EXPECT_EQ(manager.getUploadSpeedLimit(), 0);
	EXPECT_EQ(manager.getDownloadPath(), "~/Downloads");
	EXPECT_TRUE(manager.getEnableDHT());
	EXPECT_TRUE(manager.getEnableUPnP());
	EXPECT_TRUE(manager.getEnableNATPMP());
}

TEST_F(ConfigManagerTest, SaveAndLoadConfig)
{
	ConfigManager manager;
	std::string configPath = (testDir / "settings.json").string();

	// Set some values
	manager.setDownloadSpeedLimit(1024000);
	manager.setUploadSpeedLimit(512000);
	manager.setDownloadPath("/custom/downloads");
	manager.setEnableDHT(false);
	manager.setEnableUPnP(false);
	manager.setEnableNATPMP(true);

	// Save config
	manager.save(configPath);
	manager.waitForAsyncOperations();

	// Load into a new manager
	ConfigManager manager2;
	manager2.load(configPath);

	// Verify values
	EXPECT_EQ(manager2.getDownloadSpeedLimit(), 1024000);
	EXPECT_EQ(manager2.getUploadSpeedLimit(), 512000);
	EXPECT_EQ(manager2.getDownloadPath(), "/custom/downloads");
	EXPECT_FALSE(manager2.getEnableDHT());
	EXPECT_FALSE(manager2.getEnableUPnP());
	EXPECT_TRUE(manager2.getEnableNATPMP());
}

TEST_F(ConfigManagerTest, MigrateFromLegacyConfig)
{
	std::string configPath = (testDir / "legacy.json").string();

	// Create a legacy (version 0) config file
	{
		std::ofstream file(configPath);
		file << R"({
			"speed_limits": {
				"download": 2048000,
				"upload": 1024000
			},
			"download_path": "/legacy/path",
			"enable_dht": false
		})";
		file.close();
	}

	// Load it with ConfigManager
	ConfigManager manager;
	manager.load(configPath);

	// Should be migrated to version 1
	EXPECT_EQ(manager.getConfigVersion(), 1);

	// Old values should be preserved
	EXPECT_EQ(manager.getDownloadSpeedLimit(), 2048000);
	EXPECT_EQ(manager.getUploadSpeedLimit(), 1024000);
	EXPECT_EQ(manager.getDownloadPath(), "/legacy/path");
	EXPECT_FALSE(manager.getEnableDHT());

	// New defaults should be added
	EXPECT_TRUE(manager.getEnableUPnP());
	EXPECT_TRUE(manager.getEnableNATPMP());
}

TEST_F(ConfigManagerTest, ConfigVersionPersists)
{
	ConfigManager manager;
	std::string configPath = (testDir / "versioned.json").string();

	// Load default config and save
	manager.load(configPath);
	manager.save(configPath);
	manager.waitForAsyncOperations();

	// Load the saved file and check it has version
	std::ifstream file(configPath);
	json savedConfig;
	file >> savedConfig;
	file.close();

	EXPECT_TRUE(savedConfig.contains("version"));
	EXPECT_EQ(savedConfig["version"], 1);
}

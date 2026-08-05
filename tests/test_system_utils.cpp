#include <gtest/gtest.h>
#include "SystemUtils.hpp"
#include <filesystem>

namespace {

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

TEST(SystemUtilsTest, LegacyHelpersValidatePaths)
{
	EXPECT_EQ(Utils::SystemUtils::openFileExplorer("/path/that/does/not/exist").code, ResultCode::NotFound);
	EXPECT_EQ(Utils::SystemUtils::openFilePreview("/path/that/does/not/exist").code, ResultCode::NotFound);
}

} // namespace

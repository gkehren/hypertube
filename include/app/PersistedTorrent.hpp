#pragma once

#include <string>
#include <vector>

struct PersistedTorrent
{
	std::string magnetUri;
	std::string savePath;
	std::string torrentFilePath;
	std::vector<char> resumeData;
};

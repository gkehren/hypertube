#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Presentation
{
struct TorrentRowDto
{
	std::string id;
	std::string name;
	std::string stateLabel;
	std::string progressLabel;
	std::string sizeLabel;
	std::string downloadRateLabel;
	std::string uploadRateLabel;
	std::string peersLabel;
	std::string seedsLabel;
	std::string etaLabel;
	float progress = 0.0f;
	std::int64_t sizeBytes = 0;
	std::int64_t downloadRateBytes = 0;
	std::int64_t uploadRateBytes = 0;
	std::int64_t etaSeconds = -1;
	int queuePosition = -1;
	int peers = 0;
	int seeds = 0;
	bool paused = false;
	bool active = false;
	bool error = false;
	bool finished = false;
};

struct CategoryDto
{
	int id = 0;
	std::string label;
	int count = 0;
};

struct TorrentGeneralDetailsDto
{
	std::string id;
	std::string name;
	std::string stateLabel;
	std::string sizeLabel;
	std::string progressLabel;
	std::string downloadRateLabel;
	std::string uploadRateLabel;
	std::string etaLabel;
	std::string seedsPeersLabel;
	std::string downloadedLabel;
	std::string uploadedLabel;
	std::string savePath;
	float progress = 0.0f;
};

struct TorrentFileRowDto
{
	int index = -1;
	std::string name;
	std::string relativePath;
	std::string sizeLabel;
	std::string progressLabel;
	std::int64_t sizeBytes = 0;
	std::int64_t downloadedBytes = 0;
	int priority = 0;
	float progress = 0.0f;
	bool previewable = false;
};

struct TorrentPeerRowDto
{
	std::string address;
	std::string client;
	std::string flags;
	std::string downloadSpeedLabel;
	std::string uploadSpeedLabel;
};

struct TorrentTrackerRowDto
{
	std::string url;
	std::string statusLabel;
	bool verified = false;
};

enum class DetailsState
{
	Loading,
	Ready,
	Unavailable,
	Failed
};

struct TorrentDetailsDto
{
	DetailsState state = DetailsState::Loading;
	std::string message;
	std::string savePath;
	bool truncated = false;
	std::vector<TorrentFileRowDto> files;
	std::vector<TorrentPeerRowDto> peers;
	std::vector<TorrentTrackerRowDto> trackers;
};

struct SearchResultDto
{
	std::string id;
	std::string name;
	std::string sizeLabel;
	std::string seedersLabel;
	std::string leechersLabel;
	std::string ratioLabel;
	std::string completedLabel;
	std::string createdLabel;
	std::string lastSeenLabel;
	std::string category;
	std::string magnetUri;
	std::int64_t sizeBytes = 0;
	int seeders = 0;
	int leechers = 0;
	int completed = 0;
	bool favorite = false;
};

struct LogRowDto
{
	std::string timestamp;
	std::string level;
	std::string category;
	std::string message;
	int severity = 0;
};

struct NotificationStateDto
{
	bool visible = false;
	bool error = false;
	std::string message;
};
} // namespace Presentation

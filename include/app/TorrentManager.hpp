#pragma once

#include "Result.hpp"
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>
#include <optional>

// Forward declaration
struct TorrentConfigData;

// A value snapshot used by the UI and persistence layers. The map containing
// these entries never escapes TorrentManager, so callers cannot race a map
// mutation while rendering or saving the application state.
struct ManagedTorrent
{
	lt::sha1_hash hash;
	lt::torrent_handle handle;
	std::string torrentFilePath;
};

typedef enum
{
	REMOVE_TORRENT,
	REMOVE_TORRENT_FILES,
	REMOVE_TORRENT_DATA,
	REMOVE_TORRENT_FILES_AND_DATA
} RemoveTorrentType;

class TorrentManager
{
public:
	Result addTorrent(const std::string &torrentPath, const std::string &savePath = "./downloads");
	Result addMagnetTorrent(const std::string &magnetUri, const std::string &savePath = "./downloads");
	void addTorrentsFromConfig(const std::vector<TorrentConfigData> &torrents);
	Result removeTorrent(const lt::sha1_hash hash, RemoveTorrentType removeType);
	std::vector<ManagedTorrent> getTorrentSnapshot() const;

	// Speed limit methods
	void setDownloadSpeedLimit(int bytesPerSecond); // 0 means unlimited
	void setUploadSpeedLimit(int bytesPerSecond);	// 0 means unlimited
	int getDownloadSpeedLimit() const;
	int getUploadSpeedLimit() const;

	// Status cache methods
	std::optional<lt::torrent_status> getCachedStatus(const lt::sha1_hash &hash) const;
	std::shared_ptr<const std::unordered_map<lt::sha1_hash, lt::torrent_status>> getStatusCache() const;
	void refreshStatusCache();
	void setCacheRefreshInterval(int milliseconds);
	bool shouldRefreshCache() const;

	// Sequential download (streaming) methods
	void setSequentialDownload(const lt::sha1_hash &hash, bool sequential);
	bool isSequentialDownload(const lt::sha1_hash &hash) const;

	// Proxy configuration methods
	void setProxyConfig(const std::string &hostname, int port, const std::string &username = "", const std::string &password = "", int proxyType = 0);

	// Alert polling methods
	std::vector<lt::alert *> pollAlerts();

private:
	lt::session session;
	mutable std::mutex stateMutex;
	std::unordered_map<lt::sha1_hash, lt::torrent_handle> torrents;
	std::unordered_map<lt::sha1_hash, std::string> torrentFilePaths;

	// Status cache
	mutable std::mutex cacheMutex;
	std::shared_ptr<const std::unordered_map<lt::sha1_hash, lt::torrent_status>> statusCache = std::make_shared<std::unordered_map<lt::sha1_hash, lt::torrent_status>>();
	std::chrono::steady_clock::time_point lastCacheRefresh;
	int cacheRefreshIntervalMs = 250; // Default 250ms
};

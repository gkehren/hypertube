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

// Forward declaration
struct TorrentConfigData;

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
	const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &getTorrents() const;
	const std::unordered_map<lt::sha1_hash, std::string> &getTorrentFilePaths() const;

	// Speed limit methods
	void setDownloadSpeedLimit(int bytesPerSecond); // 0 means unlimited
	void setUploadSpeedLimit(int bytesPerSecond);	// 0 means unlimited
	int getDownloadSpeedLimit() const;
	int getUploadSpeedLimit() const;

private:
	lt::session session;
	std::unordered_map<lt::sha1_hash, lt::torrent_handle> torrents;
	std::unordered_map<lt::sha1_hash, std::string> torrentFilePaths;
};

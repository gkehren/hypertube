#pragma once

#include "Result.hpp"
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <string>
#include <unordered_map>

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
	void addTorrentsFromVec(const std::vector<std::string> torrents);
	Result removeTorrent(const lt::sha1_hash hash, RemoveTorrentType removeType);
	const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &getTorrents() const;

private:
	lt::session session;
	std::unordered_map<lt::sha1_hash, lt::torrent_handle> torrents;
};

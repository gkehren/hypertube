#pragma once

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <string>
#include <unordered_map>

class TorrentManager
{
	public:
		void	addTorrent(const std::string& torrentPath);
		void	addMagnetTorrent(const std::string& magnetUri);
		void	removeTorrent(const std::string& infoHash);
		std::unordered_map<lt::sha1_hash, lt::torrent_handle>&	getTorrents();

	private:
		lt::session	session;
		std::unordered_map<lt::sha1_hash, lt::torrent_handle>	torrents;
};

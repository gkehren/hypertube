#pragma once

#include <string>
#include <libtorrent/session.hpp>

class TorrentManager
{
	public:
		void	addTorrent(const std::string& torrentPath);
		void	removeTorrent(const std::string& infoHash);

	private:
		lt::session	session;
};
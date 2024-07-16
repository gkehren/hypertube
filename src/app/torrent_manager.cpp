#include "torrent_manager.hpp"
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>

void	TorrentManager::addTorrent(const std::string& torrentPath)
{
	lt::add_torrent_params	params;
	params.save_path = "./downloads";
	params.ti = std::make_shared<lt::torrent_info>(torrentPath);
	this->session.add_torrent(params);
}

void	TorrentManager::removeTorrent(const std::string& infoHash)
{
}
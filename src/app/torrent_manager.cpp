#include "torrent_manager.hpp"
#include <iostream>

void	TorrentManager::addTorrent(const std::string& torrentPath)
{
	lt::add_torrent_params	params;
	params.save_path = "./downloads";
	params.ti = std::make_shared<lt::torrent_info>(torrentPath);
	lt::torrent_handle	handle = this->session.add_torrent(params);
	this->torrents[handle.info_hash()] = handle;
}

void	TorrentManager::addMagnetTorrent(const std::string& magnetUri)
{
	try
	{
		std::cout << "Adding magnet torrent: " << magnetUri << std::endl;
		lt::add_torrent_params	params = lt::parse_magnet_uri(magnetUri);
		params.save_path = "./downloads";
		lt::torrent_handle	handle = this->session.add_torrent(params);
		this->torrents[handle.info_hash()] = handle;

		std::cout << "Added magnet torrent: " << handle.status().name << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to add magnet torrent: " << e.what() << std::endl;
	}
}

void	TorrentManager::removeTorrent(const lt::sha1_hash hash, RemoveTorrentType removeType)
{
	auto it = this->torrents.find(hash);
	if (it != this->torrents.end())
	{
		if (removeType == REMOVE_TORRENT_FILES || removeType == REMOVE_TORRENT_FILES_AND_DATA)
		{
			// TODO: Remove .torrent file (if it exists)
		}
		if (removeType == REMOVE_TORRENT_DATA || removeType == REMOVE_TORRENT_FILES_AND_DATA)
			this->session.remove_torrent(it->second, lt::session::delete_files);
		else
			this->session.remove_torrent(it->second);
		this->torrents.erase(it);
	}
}

std::unordered_map<lt::sha1_hash, lt::torrent_handle>&	TorrentManager::getTorrents()
{
	return (this->torrents);
}
#include "TorrentManager.hpp"
#include <iostream>

Result TorrentManager::addTorrent(const std::string &torrentPath, const std::string &savePath)
{
	try
	{
		lt::add_torrent_params params;
		params.save_path = savePath;
		params.ti = std::make_shared<lt::torrent_info>(torrentPath);
		lt::torrent_handle handle = this->session.add_torrent(params);
		this->torrents[handle.info_hash()] = handle;

		std::cout << "Added torrent from file: " << handle.status().name << std::endl;
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to add torrent: " << e.what() << std::endl;
		return Result::Failure(e.what());
	}
}

Result TorrentManager::addMagnetTorrent(const std::string &magnetUri, const std::string &savePath)
{
	try
	{
		std::cout << "Adding magnet torrent: " << magnetUri << std::endl;
		lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri);
		params.save_path = savePath;
		lt::torrent_handle handle = this->session.add_torrent(params);
		this->torrents[handle.info_hash()] = handle;

		std::cout << "Added magnet torrent: " << handle.status().name << std::endl;
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to add magnet torrent: " << e.what() << std::endl;
		return Result::Failure(e.what());
	}
}

void TorrentManager::addTorrentsFromVec(const std::vector<std::string> &torrents)
{
	auto it = torrents.begin();
	while (it != torrents.end())
	{
		std::string magnetUri = *it++;
		std::string savePath = *it++;
		this->addMagnetTorrent(magnetUri, savePath);
	}
}

Result TorrentManager::removeTorrent(const lt::sha1_hash hash, RemoveTorrentType removeType)
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
		return Result::Success();
	}
	else
	{
		return Result::Failure("Torrent not found");
	}
}

const std::unordered_map<lt::sha1_hash, lt::torrent_handle> &TorrentManager::getTorrents() const
{
	return this->torrents;
}

void TorrentManager::setDownloadSpeedLimit(int bytesPerSecond)
{
	lt::settings_pack settings;
	settings.set_int(lt::settings_pack::download_rate_limit, bytesPerSecond);
	session.apply_settings(settings);
}

void TorrentManager::setUploadSpeedLimit(int bytesPerSecond)
{
	lt::settings_pack settings;
	settings.set_int(lt::settings_pack::upload_rate_limit, bytesPerSecond);
	session.apply_settings(settings);
}

int TorrentManager::getDownloadSpeedLimit() const
{
	return session.get_settings().get_int(lt::settings_pack::download_rate_limit);
}

int TorrentManager::getUploadSpeedLimit() const
{
	return session.get_settings().get_int(lt::settings_pack::upload_rate_limit);
}

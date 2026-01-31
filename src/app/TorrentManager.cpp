#include "TorrentManager.hpp"
#include "ConfigManager.hpp"
#include <iostream>
#include <filesystem>

Result TorrentManager::addTorrent(const std::string &torrentPath, const std::string &savePath)
{
	try
	{
		lt::add_torrent_params params;
		params.save_path = savePath;
		params.ti = std::make_shared<lt::torrent_info>(torrentPath);
		lt::torrent_handle handle = this->session.add_torrent(params);

		lt::sha1_hash hash = handle.info_hash();
		this->torrents[hash] = handle;
		this->torrentFilePaths[hash] = torrentPath;

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

void TorrentManager::addTorrentsFromConfig(const std::vector<TorrentConfigData> &torrents)
{
	for (const auto &data : torrents)
	{
		// Try to add from file if path exists
		if (!data.torrentFilePath.empty() && std::filesystem::exists(data.torrentFilePath))
		{
			this->addTorrent(data.torrentFilePath, data.savePath);
		}
		else if (!data.magnetUri.empty())
		{
			this->addMagnetTorrent(data.magnetUri, data.savePath);
		}
	}
}

Result TorrentManager::removeTorrent(const lt::sha1_hash hash, RemoveTorrentType removeType)
{
	auto it = this->torrents.find(hash);
	if (it != this->torrents.end())
	{
		if (removeType == REMOVE_TORRENT_FILES || removeType == REMOVE_TORRENT_FILES_AND_DATA)
		{
			auto pathIt = this->torrentFilePaths.find(hash);
			if (pathIt != this->torrentFilePaths.end())
			{
				try
				{
					if (std::filesystem::exists(pathIt->second))
					{
						std::filesystem::remove(pathIt->second);
						std::cout << "Deleted .torrent file: " << pathIt->second << std::endl;
					}
				}
				catch (const std::exception &e)
				{
					std::cerr << "Failed to delete .torrent file: " << e.what() << std::endl;
				}
				this->torrentFilePaths.erase(pathIt);
			}
		}

		if (removeType == REMOVE_TORRENT_DATA || removeType == REMOVE_TORRENT_FILES_AND_DATA)
			this->session.remove_torrent(it->second, lt::session::delete_files);
		else
			this->session.remove_torrent(it->second);

		this->torrents.erase(it);
		// Also clean up path if it wasn't removed above (e.g. for other remove types)
		// Although removeType check implies we only delete file on specific flags,
		// we should probably always stop tracking the file path when the torrent is removed from session.
		if (this->torrentFilePaths.count(hash))
		{
			this->torrentFilePaths.erase(hash);
		}

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

const std::unordered_map<lt::sha1_hash, std::string> &TorrentManager::getTorrentFilePaths() const
{
	return this->torrentFilePaths;
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

const lt::torrent_status *TorrentManager::getCachedStatus(const lt::sha1_hash &hash) const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	auto it = statusCache.find(hash);
	if (it != statusCache.end())
	{
		return &it->second;
	}
	return nullptr;
}

void TorrentManager::refreshStatusCache()
{
	std::lock_guard<std::mutex> lock(cacheMutex);

	// Clear old cache
	statusCache.clear();

	// Refresh all torrent statuses
	for (const auto &[hash, handle] : torrents)
	{
		if (handle.is_valid())
		{
			statusCache[hash] = handle.status();
		}
	}

	lastCacheRefresh = std::chrono::steady_clock::now();
}

void TorrentManager::setCacheRefreshInterval(int milliseconds)
{
	cacheRefreshIntervalMs = milliseconds;
}

bool TorrentManager::shouldRefreshCache() const
{
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCacheRefresh);
	return elapsed.count() >= cacheRefreshIntervalMs;
}

// Polls alerts from the libtorrent session. Returns a vector of alert pointers.
// Note: The returned alert pointers are managed by libtorrent and should not be deleted.
// They remain valid until the next call to pop_alerts() or until the session is destroyed.
std::vector<lt::alert *> TorrentManager::pollAlerts()
{
	std::vector<lt::alert *> alerts;
	session.pop_alerts(&alerts);
	return alerts;
}

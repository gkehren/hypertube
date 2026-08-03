#include "TorrentManager.hpp"
#include "ConfigManager.hpp"
#include "Logger.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace
{
void invalidateStatusCache(std::mutex &cacheMutex, std::shared_ptr<const std::unordered_map<lt::sha1_hash, lt::torrent_status>> &cache)
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	cache = std::make_shared<std::unordered_map<lt::sha1_hash, lt::torrent_status>>();
}
}

Result TorrentManager::addTorrent(const std::string &torrentPath, const std::string &savePath)
{
	try
	{
		lt::add_torrent_params params;
		params.save_path = savePath;
		params.ti = std::make_shared<lt::torrent_info>(torrentPath);
		lt::torrent_handle handle = this->session.add_torrent(params);

		lt::sha1_hash hash = handle.info_hash();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (torrents.find(hash) != torrents.end())
				return Result::Failure("Torrent is already added");
			torrents.emplace(hash, handle);
			torrentFilePaths.emplace(hash, torrentPath);
		}

		std::cout << "Added torrent from file: " << handle.status().name << std::endl;
		Utils::Logger::info("torrent", "Added torrent from file: " + torrentPath);
		invalidateStatusCache(cacheMutex, statusCache);
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to add torrent: " << e.what() << std::endl;
		Utils::Logger::error("torrent", "Failed to add torrent: " + std::string(e.what()));
		return Result::Failure(e.what());
	}
}

Result TorrentManager::addMagnetTorrent(const std::string &magnetUri, const std::string &savePath)
{
	try
	{
		lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri);
		std::cout << "Adding magnet torrent: " << params.name << " (hash: " << params.info_hashes.v1 << ")" << std::endl;
		params.save_path = savePath;
		lt::torrent_handle handle = this->session.add_torrent(params);
		const lt::sha1_hash hash = handle.info_hash();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (torrents.find(hash) != torrents.end())
				return Result::Failure("Torrent is already added");
			torrents.emplace(hash, handle);
		}

		std::cout << "Added magnet torrent: " << handle.status().name << std::endl;
		Utils::Logger::info("torrent", "Added torrent from magnet URI");
		invalidateStatusCache(cacheMutex, statusCache);
		return Result::Success();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to add magnet torrent: " << e.what() << std::endl;
		Utils::Logger::error("torrent", "Failed to add magnet torrent: " + std::string(e.what()));
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
	lt::torrent_handle handle;
	std::string torrentFilePath;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it == torrents.end())
			return Result::Failure("Torrent not found");
		handle = it->second;
		auto pathIt = torrentFilePaths.find(hash);
		if (pathIt != torrentFilePaths.end())
			torrentFilePath = pathIt->second;
	}

	if (handle.is_valid())
	{
		if (removeType == REMOVE_TORRENT_FILES || removeType == REMOVE_TORRENT_FILES_AND_DATA)
		{
			if (!torrentFilePath.empty())
			{
				try
				{
					if (std::filesystem::exists(torrentFilePath))
					{
						std::filesystem::remove(torrentFilePath);
						std::cout << "Deleted .torrent file: " << torrentFilePath << std::endl;
					}
				}
				catch (const std::exception &e)
				{
					std::cerr << "Failed to delete .torrent file: " << e.what() << std::endl;
					Utils::Logger::warning("torrent", "Failed to delete .torrent file: " + std::string(e.what()));
				}
			}
		}

		if (removeType == REMOVE_TORRENT_DATA || removeType == REMOVE_TORRENT_FILES_AND_DATA)
			session.remove_torrent(handle, lt::session::delete_files);
		else
			session.remove_torrent(handle);

		{
			std::lock_guard<std::mutex> lock(stateMutex);
			torrents.erase(hash);
			torrentFilePaths.erase(hash);
		}
		invalidateStatusCache(cacheMutex, statusCache);
		Utils::Logger::info("torrent", "Removed torrent " + hash.to_string());

		return Result::Success();
	}
	return Result::Failure("Torrent handle is invalid");
}

std::vector<ManagedTorrent> TorrentManager::getTorrentSnapshot() const
{
	std::vector<ManagedTorrent> snapshot;
	std::lock_guard<std::mutex> lock(stateMutex);
	snapshot.reserve(torrents.size());
	for (const auto &[hash, handle] : torrents)
	{
		ManagedTorrent entry{hash, handle, {}};
		auto pathIt = torrentFilePaths.find(hash);
		if (pathIt != torrentFilePaths.end())
			entry.torrentFilePath = pathIt->second;
		snapshot.push_back(std::move(entry));
	}
	return snapshot;
}

void TorrentManager::setDownloadSpeedLimit(int bytesPerSecond)
{
	lt::settings_pack settings;
	settings.set_int(lt::settings_pack::download_rate_limit, std::max(bytesPerSecond, 0));
	session.apply_settings(settings);
}

void TorrentManager::setUploadSpeedLimit(int bytesPerSecond)
{
	lt::settings_pack settings;
	settings.set_int(lt::settings_pack::upload_rate_limit, std::max(bytesPerSecond, 0));
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

std::optional<lt::torrent_status> TorrentManager::getCachedStatus(const lt::sha1_hash &hash) const
{
	auto cache = getStatusCache();
	if (cache)
	{
		auto it = cache->find(hash);
		if (it != cache->end())
		{
			return it->second;
		}
	}
	return std::nullopt;
}

std::shared_ptr<const std::unordered_map<lt::sha1_hash, lt::torrent_status>> TorrentManager::getStatusCache() const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	return statusCache;
}

void TorrentManager::refreshStatusCache()
{
	auto newCache = std::make_shared<std::unordered_map<lt::sha1_hash, lt::torrent_status>>();
	const auto torrentsSnapshot = getTorrentSnapshot();

	// Refresh all torrent statuses
	for (const auto &torrent : torrentsSnapshot)
	{
		if (torrent.handle.is_valid())
		{
			(*newCache)[torrent.hash] = torrent.handle.status();
		}
	}

	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		statusCache = std::move(newCache);
	}

	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		lastCacheRefresh = std::chrono::steady_clock::now();
	}
}

void TorrentManager::setCacheRefreshInterval(int milliseconds)
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	cacheRefreshIntervalMs = std::max(milliseconds, 16);
}

bool TorrentManager::shouldRefreshCache() const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
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

void TorrentManager::setSequentialDownload(const lt::sha1_hash &hash, bool sequential)
{
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it != torrents.end())
			handle = it->second;
	}
	if (handle.is_valid())
	{
		if (sequential)
			handle.set_flags(lt::torrent_flags::sequential_download);
		else
			handle.unset_flags(lt::torrent_flags::sequential_download);
	}
}

bool TorrentManager::isSequentialDownload(const lt::sha1_hash &hash) const
{
	lt::torrent_handle handle;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it != torrents.end())
			handle = it->second;
	}
	if (handle.is_valid())
	{
		return (handle.flags() & lt::torrent_flags::sequential_download) != lt::torrent_flags_t{};
	}
	return false;
}

void TorrentManager::setProxyConfig(const std::string &hostname, int port, const std::string &username, const std::string &password, int proxyType)
{
	lt::settings_pack settings;
	settings.set_str(lt::settings_pack::proxy_hostname, hostname);
	settings.set_int(lt::settings_pack::proxy_port, port);
	settings.set_str(lt::settings_pack::proxy_username, username);
	settings.set_str(lt::settings_pack::proxy_password, password);
	settings.set_int(lt::settings_pack::proxy_type, static_cast<lt::settings_pack::proxy_type_t>(proxyType));
	session.apply_settings(settings);
}

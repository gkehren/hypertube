#include "TorrentManager.hpp"
#include "ConfigManager.hpp"
#include "Logger.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace
{
void invalidateStatusCache(std::mutex &cacheMutex, std::shared_ptr<const std::unordered_map<lt::info_hash_t, lt::torrent_status>> &cache)
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	cache = std::make_shared<std::unordered_map<lt::info_hash_t, lt::torrent_status>>();
}

std::string hashForLog(const lt::info_hash_t &hash)
{
	if (hash.has_v1())
		return hash.v1.to_string();
	if (hash.has_v2())
		return hash.v2.to_string();
	return "unknown";
}

Result validateAddPaths(const std::string &savePath, const std::string *torrentPath = nullptr)
{
	std::error_code error;
	if (torrentPath)
	{
		const std::filesystem::path source(*torrentPath);
		if (!std::filesystem::is_regular_file(source, error))
			return Result::Failure("Torrent file does not exist or is not a regular file", ResultCode::InvalidInput);
	}

	if (savePath.empty())
		return Result::Failure("Download path cannot be empty", ResultCode::InvalidInput);
	const std::filesystem::path destination(savePath);
	std::filesystem::create_directories(destination, error);
	if (error || !std::filesystem::is_directory(destination, error))
		return Result::Failure("Download path cannot be created or accessed: " + savePath, ResultCode::Storage);
	return Result::Success();
}
}

Result TorrentManager::addTorrent(const std::string &torrentPath, const std::string &savePath)
{
	Result validation = validateAddPaths(savePath, &torrentPath);
	if (!validation)
		return validation;
	std::lock_guard<std::mutex> operationLock(operationMutex);
	try
	{
		lt::add_torrent_params params;
		params.save_path = savePath;
		params.ti = std::make_shared<lt::torrent_info>(torrentPath);
		params.flags |= lt::torrent_flags::duplicate_is_error;
		const lt::info_hash_t expectedHash = params.ti->info_hashes();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if ((expectedHash.has_v1() || expectedHash.has_v2()) && torrents.find(expectedHash) != torrents.end())
				return Result::Failure("Torrent is already added", ResultCode::Duplicate);
		}
		lt::torrent_handle handle = this->session.add_torrent(params);

		lt::info_hash_t hash = handle.info_hashes();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
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
	Result validation = validateAddPaths(savePath);
	if (!validation)
		return validation;
	if (magnetUri.rfind("magnet:?", 0) != 0)
		return Result::Failure("Invalid magnet URI", ResultCode::InvalidInput);
	std::lock_guard<std::mutex> operationLock(operationMutex);
	try
	{
		lt::add_torrent_params params = lt::parse_magnet_uri(magnetUri);
		if (!params.info_hashes.has_v1() && !params.info_hashes.has_v2())
			return Result::Failure("Magnet URI does not contain a supported info hash", ResultCode::InvalidInput);
		std::cout << "Adding magnet torrent: " << params.name << " (hash: " << params.info_hashes.v1 << ")" << std::endl;
		params.save_path = savePath;
		params.flags |= lt::torrent_flags::duplicate_is_error;
		if (params.info_hashes.has_v1() || params.info_hashes.has_v2())
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (torrents.find(params.info_hashes) != torrents.end())
				return Result::Failure("Torrent is already added", ResultCode::Duplicate);
		}
		lt::torrent_handle handle = this->session.add_torrent(params);
		const lt::info_hash_t hash = handle.info_hashes();
		{
			std::lock_guard<std::mutex> lock(stateMutex);
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
		if (!data.resumeData.empty())
		{
			std::lock_guard<std::mutex> operationLock(operationMutex);
			try
			{
				lt::add_torrent_params params = lt::read_resume_data(data.resumeData);
				if (!data.savePath.empty())
					params.save_path = data.savePath;
				params.flags |= lt::torrent_flags::duplicate_is_error;
				lt::torrent_handle handle = session.add_torrent(params);
				const lt::info_hash_t hash = handle.info_hashes();
				{
					std::lock_guard<std::mutex> lock(stateMutex);
					this->torrents.emplace(hash, handle);
					if (!data.torrentFilePath.empty())
						torrentFilePaths.emplace(hash, data.torrentFilePath);
				}
				Utils::Logger::info("torrent", "Restored torrent from fast-resume data");
				continue;
			}
			catch (const std::exception &e)
			{
				Utils::Logger::warning("torrent", "Fast-resume data was rejected; using the persisted source: " + std::string(e.what()));
			}
		}
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

Result TorrentManager::removeTorrent(const lt::info_hash_t &hash, RemoveTorrentType removeType)
{
	std::lock_guard<std::mutex> operationLock(operationMutex);
	lt::torrent_handle handle;
	std::string torrentFilePath;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		auto it = torrents.find(hash);
		if (it == torrents.end())
			return Result::Failure("Torrent not found", ResultCode::NotFound);
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
		Utils::Logger::info("torrent", "Removed torrent " + hashForLog(hash));

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
		ManagedTorrent entry{hash, handle, {}, {}};
		auto pathIt = torrentFilePaths.find(hash);
		if (pathIt != torrentFilePaths.end())
			entry.torrentFilePath = pathIt->second;
		snapshot.push_back(std::move(entry));
	}
	return snapshot;
}

Result TorrentManager::getPersistenceSnapshot(std::vector<ManagedTorrent> &snapshot, std::chrono::milliseconds timeout)
{
	std::lock_guard<std::mutex> operationLock(operationMutex);
	snapshot = getTorrentSnapshot();
	std::unordered_set<lt::info_hash_t> pending;
	for (const auto &torrent : snapshot)
	{
		if (!torrent.handle.is_valid())
			continue;
		try
		{
			torrent.handle.save_resume_data();
			pending.insert(torrent.hash);
		}
		catch (const std::exception &e)
		{
			Utils::Logger::warning("torrent", "Unable to request fast-resume data: " + std::string(e.what()));
		}
	}

	const auto deadline = std::chrono::steady_clock::now() + timeout;
	while (!pending.empty() && std::chrono::steady_clock::now() < deadline)
	{
		session.wait_for_alert(lt::milliseconds(100));
		std::vector<lt::alert *> alerts;
		session.pop_alerts(&alerts);
		for (lt::alert *alert : alerts)
		{
			if (auto *saved = lt::alert_cast<lt::save_resume_data_alert>(alert))
			{
				const lt::info_hash_t hash = saved->handle.info_hashes();
				for (auto &torrent : snapshot)
				{
					if (torrent.hash == hash)
					{
						torrent.resumeData = lt::write_resume_data_buf(saved->params);
						break;
					}
				}
				pending.erase(hash);
			}
			else if (auto *failed = lt::alert_cast<lt::save_resume_data_failed_alert>(alert))
			{
				pending.erase(failed->handle.info_hashes());
				Utils::Logger::warning("torrent", "Unable to save fast-resume data: " + failed->error.message());
			}
		}
	}

	if (!pending.empty())
		return Result::Failure("Timed out while collecting fast-resume data for " + std::to_string(pending.size()) + " torrent(s)", ResultCode::Storage, true);
	return Result::Success();
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

void TorrentManager::configureDiscovery(bool enableDht, bool enableUpnp, bool enableNatPmp)
{
	lt::settings_pack settings;
	settings.set_bool(lt::settings_pack::enable_dht, enableDht);
	settings.set_bool(lt::settings_pack::enable_upnp, enableUpnp);
	settings.set_bool(lt::settings_pack::enable_natpmp, enableNatPmp);
	session.apply_settings(settings);
}

std::optional<lt::torrent_status> TorrentManager::getCachedStatus(const lt::info_hash_t &hash) const
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

std::shared_ptr<const std::unordered_map<lt::info_hash_t, lt::torrent_status>> TorrentManager::getStatusCache() const
{
	std::lock_guard<std::mutex> lock(cacheMutex);
	return statusCache;
}

void TorrentManager::refreshStatusCache()
{
	auto newCache = std::make_shared<std::unordered_map<lt::info_hash_t, lt::torrent_status>>();
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

void TorrentManager::setSequentialDownload(const lt::info_hash_t &hash, bool sequential)
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

bool TorrentManager::isSequentialDownload(const lt::info_hash_t &hash) const
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
	lt::settings_pack::proxy_type_t libtorrentProxyType = lt::settings_pack::none;
	if (proxyType == 1)
		libtorrentProxyType = username.empty() ? lt::settings_pack::socks5 : lt::settings_pack::socks5_pw;
	else if (proxyType == 2)
		libtorrentProxyType = username.empty() ? lt::settings_pack::http : lt::settings_pack::http_pw;
	settings.set_int(lt::settings_pack::proxy_type, libtorrentProxyType);
	session.apply_settings(settings);
}
